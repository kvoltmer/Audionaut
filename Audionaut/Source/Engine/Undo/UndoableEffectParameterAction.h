//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Engine/Effects/InsertChain.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Channel/AudioChannel.h"

namespace audium
{

/**
 * Where an insert chain lives: a bus channel of a track, a track's own
 * inserts, or the master. Resolved against the container at apply time, so
 * the address stays valid across rebuilds that replace the objects.
 */
struct InsertChainAddress
{
    enum class Kind { channel, track, master };

    Kind kind = Kind::master;
    int trackId = -1;
    int channelNumber = -1;

    static InsertChainAddress ofChannel (int trackId, int channelNumber) { return { Kind::channel, trackId, channelNumber }; }
    static InsertChainAddress ofTrack (int trackId) { return { Kind::track, trackId, -1 }; }
    static InsertChainAddress ofMaster() { return { Kind::master, -1, -1 }; }

    InsertChain* resolve (AudioTrackContainer& container) const
    {
        if (kind == Kind::master)
            return &container.getMasterInserts();

        auto track = container.getAudioTrack (trackId);
        if (track == nullptr)
            return nullptr;
        if (kind == Kind::track)
            return &track->getTrackInserts();

        auto channel = track->getChannel (channelNumber);
        return channel != nullptr ? &channel->inserts : nullptr;
    }
};

/**
 * Undo step for one effect parameter: the old and the new value, applied
 * straight into the effect's atomics, so no plan is rebuilt and playback
 * continues undisturbed. A drag records one step from its start to its end.
 */
struct UndoableEffectParameterAction final : public juce::UndoableAction
{
    UndoableEffectParameterAction (AudioTrackContainer& container_, InsertChainAddress address_, int slotIndex_, int parameterIndex_, float oldValue_, float newValue_) noexcept :
        container (container_),
        address (address_),
        slotIndex (slotIndex_),
        parameterIndex (parameterIndex_),
        oldValue (oldValue_),
        newValue (newValue_)
    {
    }

    bool perform() override { return apply (newValue); }
    bool undo() override { return apply (oldValue); }
    int getSizeInUnits() override { return 1; }

private:
    bool apply (float value)
    {
        auto* chain = address.resolve (container);
        if (chain == nullptr)
            return false;
        auto slot = chain->getSlot (slotIndex);
        if (slot == nullptr)
            return false;
        slot->getEffect().setParameter (parameterIndex, value);
        return true;
    }

    AudioTrackContainer& container;
    const InsertChainAddress address;
    const int slotIndex;
    const int parameterIndex;
    const float oldValue;
    const float newValue;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UndoableEffectParameterAction)
};

} // namespace audium
