//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "MixerPlanner.h"

#include "InsertChain.h"
#include "Engine/Channel/AudioChannel.h"
#include "Engine/Core/HeadlessMode.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"

namespace audium
{

MixerPlanner::MixerPlanner (std::shared_ptr<EffectRegistry> registry_, std::shared_ptr<MixPlanContainer> plans_) :
    registry (std::move (registry_)),
    plans (std::move (plans_))
{
}

MixerPlanner::~MixerPlanner()
{
    cancelPendingUpdate();
}

void MixerPlanner::setSource (AudioTrackContainer* container) noexcept
{
    audioTrackContainer = container;
    if (audioTrackContainer == nullptr)
        cancelPendingUpdate();
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

    if (rebuildsInline())
        rebuildNow();
    else
        triggerAsyncUpdate();
}

void MixerPlanner::flushPendingRebuild()
{
    handleUpdateNowIfNeeded();
}

void MixerPlanner::handleAsyncUpdate()
{
    rebuildNow();
}

bool MixerPlanner::rebuildsInline() const noexcept
{
    return rebuildsSynchronously.value_or (HeadlessMode::isHeadless());
}

void MixerPlanner::prepare (double sampleRate_, int maxBlockSize_)
{
    if (sampleRate_ <= 0.0 || maxBlockSize_ <= 0)
        return;

    // a shorter block than prepared for needs nothing; a longer one, or
    // another rate, makes every slot's format stale and rebuildNow replaces them
    if (sampleRate != sampleRate_ || maxBlockSize_ > maxBlockSize)
    {
        sampleRate = sampleRate_;
        maxBlockSize = maxBlockSize_;
    }

    cancelPendingUpdate();
    rebuildNow();
}

void MixerPlanner::rebuildNow()
{
    cancelPendingUpdate();
    if (audioTrackContainer == nullptr)
        return;

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
            collect (channel->inserts, 1, entry.inserts, *plan);
            plan->channels.push_back (std::move (entry));
            ++busChannel;
        }

        if (numChannels > 0 && busChannel <= MAX_AUDIO_CHANNELS)
        {
            MixPlan::Track entry;
            entry.firstChannel = busChannel - numChannels;
            entry.numChannels = numChannels;
            collect (track->getTrackInserts(), numChannels, entry.inserts, *plan);
            plan->trackAtChannel[static_cast<size_t> (entry.firstChannel)] = static_cast<int> (plan->tracks.size());
            plan->tracks.push_back (std::move (entry));
        }
    }

    collect (audioTrackContainer->getMasterInserts(), 2, plan->master, *plan);

    for (auto& slot : plan->keepAlive)
        slot->markPublished();

    plans->commit (std::move (plan));
    ++numRebuilds;
}

void MixerPlanner::collect (InsertChain& chain, int numChannels, std::vector<EffectSlot*>& into, MixPlan& plan)
{
    const auto prepared = sampleRate > 0.0 && maxBlockSize > 0;

    for (auto i = 0; i < chain.size(); ++i)
    {
        auto slot = chain.getSlot (i);

        if (prepared && ! slot->isPreparedFor (sampleRate, maxBlockSize, numChannels))
        {
            if (slot->isPublished())
            {
                // live in a plan the audio thread may be inside: a fresh
                // instance takes over, the old one goes with the retired plan
                auto replacement = replaceSlot (*slot);
                if (replacement == nullptr)
                    continue;
                chain.replaceSlot (i, replacement);
                slot = std::move (replacement);
            }
            slot->prepare (sampleRate, maxBlockSize, numChannels);
        }

        if (! slot->isPrepared())
            continue;   // no format yet (no device, no bounce): nothing to run

        into.push_back (slot.get());
        plan.keepAlive.push_back (slot);
    }
}

std::shared_ptr<EffectSlot> MixerPlanner::replaceSlot (const EffectSlot& old) const
{
    auto slot = createSlot (old.getEffect().getType().id);
    if (slot == nullptr)
        return nullptr;
    slot->getEffect().setState (old.getEffect().getState());
    slot->setEnabled (old.isEnabled());
    return slot;
}

} // namespace audium
