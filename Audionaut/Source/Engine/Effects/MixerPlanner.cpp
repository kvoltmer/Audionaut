//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "MixerPlanner.h"

#include <algorithm>

#include "InsertChain.h"
#include "Engine/Channel/AudioChannel.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Playback/AudioBusInterface.h"

namespace audium
{

MixerPlanner::MixerPlanner (std::shared_ptr<EffectRegistry> registry_, std::shared_ptr<MixPlanContainer> plans_) :
    registry (std::move (registry_)),
    plans (std::move (plans_))
{
    // a retired plan freed by the timer may have been the last to run a
    // slot that waits for its new format
    plans->onRetiredReleased = [this]
    {
        if (prepareDeferred)
            requestRebuild();
    };
}

MixerPlanner::~MixerPlanner()
{
    plans->onRetiredReleased = nullptr;
}

void MixerPlanner::setSource (AudioTrackContainer* container) noexcept
{
    audioTrackContainer = container;
}

std::shared_ptr<EffectSlot> MixerPlanner::createSlot (const std::string& typeId) const
{
    auto effect = registry->create (typeId);
    if (effect == nullptr)
        return nullptr;
    return std::make_shared<EffectSlot> (std::move (effect));
}

void MixerPlanner::requestRebuild()
{
    if (audioTrackContainer == nullptr)
        return;

    if (batchDepth > 0)
    {
        rebuildPending = true;
        return;
    }

    rebuildNow();
}

void MixerPlanner::beginBatch() noexcept
{
    ++batchDepth;
}

void MixerPlanner::endBatch()
{
    jassert (batchDepth > 0);
    if (--batchDepth == 0 && rebuildPending)
        rebuildNow();
}

void MixerPlanner::prepare (double sampleRate_, int maxBlockSize_)
{
    if (sampleRate_ <= 0.0 || maxBlockSize_ <= 0)
        return;

    // the format is the one the renderer was prepared with, no more: another
    // block size, smaller as well as larger (a bounce's, the device's own
    // after it), or another rate makes every slot's format stale and the
    // rebuild prepares them again - in place: nothing renders now
    const auto unchanged = sampleRate == sampleRate_ && maxBlockSize == maxBlockSize_;
    if (unchanged && plans->getCurrent() != nullptr)
        return;

    maxBlockSize = maxBlockSize_;
    sampleRate = sampleRate_;
    rebuild (true);
}

void MixerPlanner::rebuildNow()
{
    rebuild (false);
}

void MixerPlanner::rebuild (bool nothingRenders)
{
    rebuildPending = false;
    if (audioTrackContainer == nullptr)
        return;

    prepareDeferred = false;
    auto plan = std::make_unique<MixPlan>();
    plan->sampleRate = sampleRate;
    plan->maxBlockSize = maxBlockSize;

    const auto prepared = sampleRate > 0.0 && maxBlockSize > 0;
    juce::dsp::ProcessSpec stereoSpec { sampleRate, static_cast<juce::uint32> (juce::jmax (1, maxBlockSize)), 2 };

    // a gain that starts at its target: prepare() resets the ramp to it
    const auto seedGain = [&] (juce::dsp::Gain<float>& gain, float target)
    {
        gain.setRampDurationSeconds (gainRampSeconds);
        gain.setGainLinear (target);
        if (prepared)
            gain.prepare (stereoSpec);
    };

    // the aux buses first: the sends refer to them by index
    const auto anyAuxSolo = [&]
    {
        for (const auto& bus : audioTrackContainer->getAuxBuses().getBuses())
            if (bus->getSolo())
                return true;
        return false;
    }();

    for (const auto& bus : audioTrackContainer->getAuxBuses().getBuses())
    {
        MixPlan::Aux entry;
        entry.id = bus->getId();
        entry.state = bus->getState().get();
        plan->keepAliveAuxes.push_back (bus->getState());
        collect (bus->getInserts(), 2, entry.inserts, *plan, nothingRenders);
        float left = 0.0f, right = 0.0f;
        MixPlan::auxGainsOf (*entry.state, anyAuxSolo, left, right);
        seedGain (entry.gainLeft, left);
        seedGain (entry.gainRight, right);
        plan->auxes.push_back (std::move (entry));
    }

    const auto auxIndexOf = [&plan] (int auxId)
    {
        for (size_t a = 0; a < plan->auxes.size(); ++a)
            if (plan->auxes[a].id == auxId)
                return static_cast<int> (a);
        return -1;
    };

    auto busChannel = 0;
    for (const auto& track : audioTrackContainer->getAudioTracks())
    {
        const auto numChannels = track->getNumAudioTrackChannels();

        for (const auto& channel : track->audioChannelContainer->getObjects())
        {
            if (busChannel >= MAX_AUDIO_CHANNELS)
                break;
            MixPlan::Channel entry;

            // one send buffer pair per send into a bus that exists, handed
            // out in bus channel order, which is track order: the aux sums
            // its pairs in that order
            for (const auto& send : channel->sends.getEntries())
            {
                const auto aux = auxIndexOf (send.auxId);
                if (aux < 0 || send.state == nullptr)
                    continue;
                MixPlan::Send planned;
                planned.aux = aux;
                planned.pair = plan->numSendPairs++;
                planned.state = send.state.get();
                seedGain (planned.gain, MixPlan::sendGainOf (*planned.state));
                plan->keepAliveSends.push_back (send.state);
                plan->auxes[static_cast<size_t> (aux)].pairs.push_back (planned.pair);
                entry.sends.push_back (std::move (planned));
            }

            plan->channels.push_back (std::move (entry));
            ++busChannel;
        }

        if (numChannels > 0 && busChannel <= MAX_AUDIO_CHANNELS)
        {
            MixPlan::Track entry;
            entry.id = track->getId();
            entry.firstChannel = busChannel - numChannels;
            entry.numChannels = numChannels;
            collect (track->getTrackInserts(), numChannels, entry.inserts, *plan, nothingRenders);
            plan->trackAtChannel[static_cast<size_t> (entry.firstChannel)] = static_cast<int> (plan->tracks.size());
            plan->tracks.push_back (std::move (entry));
        }
    }

    collect (audioTrackContainer->getMasterInserts(), 2, plan->master, *plan, nothingRenders);

    // the buffers between the stages, sized now so the render thread
    // allocates nothing: what the sends write, what the aux returns are
    if (prepared)
    {
        if (plan->numSendPairs > 0)
            plan->sendOutput.setSize (2 * plan->numSendPairs, maxBlockSize);
        if (! plan->auxes.empty())
            plan->auxOutput.setSize (2 * static_cast<int> (plan->auxes.size()), maxBlockSize);

        planLatency (*plan);
    }

    // the render thread takes the plan over at the top of a block, behind
    // whatever channel data the same edit posted before it
    const auto* published = plans->commit (std::move (plan));
    auto* container = plans.get();
    audioTrackContainer->getAudioBusInterface()->postCommand ([container, published] { container->adopt (published); });

    // pumped on the spot where no device runs: the previous plan is free
    // already; otherwise the container's timer frees it after the adoption
    plans->releaseRetired();
    ++numRebuilds;

    // a slot that waited for the previous plan to go is free now: once
    // more, with it prepared (the timer's release comes back through
    // onRetiredReleased otherwise)
    if (prepareDeferred && plans->getNumRetired() == 0)
        rebuild (nothingRenders);
}

void MixerPlanner::planLatency (MixPlan& plan)
{
    // latency adds along a path; where paths meet, each waits for the latest
    const auto latencyOf = [] (const std::vector<EffectSlot*>& slots)
    {
        auto samples = 0;
        for (const auto* slot : slots)
            samples += slot->getLatencySamples();
        return samples;
    };

    // the fader point: every track is as late as its inserts, and all of
    // them wait for the latest
    std::vector<int> trackLatency (plan.tracks.size(), 0);
    std::vector<int> channelOwner (plan.channels.size(), -1);   // index into plan.tracks
    for (size_t t = 0; t < plan.tracks.size(); ++t)
    {
        const auto& track = plan.tracks[t];
        trackLatency[t] = latencyOf (track.inserts);
        for (auto k = track.firstChannel; k < track.firstChannel + track.numChannels && k < static_cast<int> (plan.channels.size()); ++k)
            channelOwner[static_cast<size_t> (k)] = static_cast<int> (t);
    }
    auto& latency = plan.latency;
    latency.preFader = trackLatency.empty() ? 0 : *std::max_element (trackLatency.begin(), trackLatency.end());

    // the aux returns: the dry main mix and the quicker returns wait for the latest aux chain
    std::vector<int> auxLatency (plan.auxes.size(), 0);
    for (size_t a = 0; a < plan.auxes.size(); ++a)
        auxLatency[a] = latencyOf (plan.auxes[a].inserts);
    latency.aux = auxLatency.empty() ? 0 : *std::max_element (auxLatency.begin(), auxLatency.end());

    latency.master = latencyOf (plan.master);

    // the waits, each on the job that owns the waiting signal; none where
    // there is nothing to wait for, so a plan without latency has no delays.
    // What this plan waits for is recorded for the plan after it
    std::vector<CarriedDelay> carrying;
    for (size_t t = 0; t < plan.tracks.size(); ++t)
    {
        auto& track = plan.tracks[t];
        track.preFaderDelay = delayFor (CarriedDelay::Kind::preFader, track.id, -1, -1, track.numChannels, latency.preFader - trackLatency[t], carrying);
    }
    const auto afterFader = latency.aux + latency.master;   // what a direct out skips
    for (size_t k = 0; k < plan.channels.size(); ++k)
    {
        const auto owner = channelOwner[k];
        const auto trackId = owner >= 0 ? plan.tracks[static_cast<size_t> (owner)].id : -1;
        const auto channelInTrack = owner >= 0 ? static_cast<int> (k) - plan.tracks[static_cast<size_t> (owner)].firstChannel : static_cast<int> (k);
        plan.channels[k].directOutDelay = delayFor (CarriedDelay::Kind::directOut, trackId, channelInTrack, -1, 1, afterFader, carrying);
    }
    for (size_t a = 0; a < plan.auxes.size(); ++a)
        plan.auxes[a].returnDelay = delayFor (CarriedDelay::Kind::auxReturn, -1, -1, plan.auxes[a].id, 2, latency.aux - auxLatency[a], carrying);
    plan.mainDryDelay = delayFor (CarriedDelay::Kind::mainDry, -1, -1, -1, 2, latency.aux, carrying);

    carriedDelays = std::move (carrying);
}

std::shared_ptr<PdcDelay> MixerPlanner::delayFor (CarriedDelay::Kind kind, int trackId, int channel, int auxId, int numChannels, int delaySamples,
                                                  std::vector<CarriedDelay>& carrying)
{
    if (delaySamples <= 0)
        return nullptr;

    std::shared_ptr<PdcDelay> delay;
    for (const auto& carried : carriedDelays)
    {
        if (carried.kind == kind && carried.trackId == trackId && carried.channel == channel && carried.auxId == auxId
            && carried.delay->matches (numChannels, delaySamples, maxBlockSize))
        {
            delay = carried.delay;   // the same wait as before: the signal in flight goes on
            break;
        }
    }
    if (delay == nullptr)
        delay = std::make_shared<PdcDelay> (numChannels, delaySamples, maxBlockSize);

    carrying.push_back ({ kind, trackId, channel, auxId, delay });
    return delay;
}

void MixerPlanner::collect (InsertChain& chain, int numChannels, std::vector<EffectSlot*>& into, MixPlan& plan, bool nothingRenders)
{
    const auto prepared = sampleRate > 0.0 && maxBlockSize > 0;

    for (auto i = 0; i < chain.size(); ++i)
    {
        auto slot = chain.getSlot (i);

        if (prepared && ! slot->isPreparedFor (sampleRate, maxBlockSize, numChannels))
        {
            if (slot->isInAPlan() && ! nothingRenders)
            {
                // the render thread may be inside it: the slot sits this
                // plan out and is prepared once the plans that ran it are gone
                prepareDeferred = true;
                continue;
            }
            slot->prepare (sampleRate, maxBlockSize, numChannels);
        }

        if (! slot->isPrepared())
            continue;   // no format yet (no device, no bounce): nothing to run

        into.push_back (slot.get());
        plan.retain (slot);
    }
}

} // namespace audium
