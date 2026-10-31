//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "MixerPlanner.h"

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

    // a block within what the slots are prepared for needs nothing (a
    // bounce's shorter last block, the device's own after a bounce); a
    // larger one, or another rate, makes every slot's format stale and
    // the rebuild prepares them again - in place: nothing renders now
    const auto unchanged = sampleRate == sampleRate_ && maxBlockSize_ <= maxBlockSize;
    if (unchanged && plans->getCurrent() != nullptr)
        return;

    maxBlockSize = sampleRate == sampleRate_ ? juce::jmax (maxBlockSize, maxBlockSize_) : maxBlockSize_;
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

    auto busChannel = 0;
    for (const auto& track : audioTrackContainer->getAudioTracks())
    {
        const auto numChannels = track->getNumAudioTrackChannels();

        for (const auto& channel : track->audioChannelContainer->getObjects())
        {
            if (busChannel >= MAX_AUDIO_CHANNELS)
                break;
            MixPlan::Channel entry;
            collect (channel->inserts, 1, entry.inserts, *plan, nothingRenders);
            plan->channels.push_back (std::move (entry));
            ++busChannel;
        }

        if (numChannels > 0 && busChannel <= MAX_AUDIO_CHANNELS)
        {
            MixPlan::Track entry;
            entry.firstChannel = busChannel - numChannels;
            entry.numChannels = numChannels;
            collect (track->getTrackInserts(), numChannels, entry.inserts, *plan, nothingRenders);
            plan->trackAtChannel[static_cast<size_t> (entry.firstChannel)] = static_cast<int> (plan->tracks.size());
            plan->tracks.push_back (std::move (entry));
        }
    }

    collect (audioTrackContainer->getMasterInserts(), 2, plan->master, *plan, nothingRenders);

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
