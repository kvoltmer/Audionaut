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
 * Where an insert chain lives: a track, an aux bus or the master. Resolved
 * against the container at apply time, so the address stays valid across
 * rebuilds that replace the objects.
 */
struct InsertChainAddress
{
    enum class Kind { track, aux, master };

    Kind kind = Kind::master;
    int id = -1;             // the track id for Kind::track, the aux id for Kind::aux

    static InsertChainAddress ofTrack (int trackId) { return { Kind::track, trackId }; }
    static InsertChainAddress ofAux (int auxId) { return { Kind::aux, auxId }; }
    static InsertChainAddress ofMaster() { return { Kind::master, -1 }; }

    InsertChain* resolve (AudioTrackContainer& container) const
    {
        if (kind == Kind::master)
            return &container.getMasterInserts();

        if (kind == Kind::aux)
        {
            auto bus = container.getAuxBuses().get (id);
            return bus != nullptr ? &bus->getInserts() : nullptr;
        }

        auto track = container.getAudioTrack (id);
        return track != nullptr ? &track->getTrackInserts() : nullptr;
    }
};

/**
 * Undo step for one effect parameter: the old and the new value, applied
 * through the chain into the effect's atomics, so no plan is rebuilt and
 * playback continues undisturbed - unless the value moves the effect's
 * latency, which the chain answers with a new plan. A drag records one
 * step from its start to its end.
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
        return chain->setParameter (slotIndex, parameterIndex, value);
    }

    AudioTrackContainer& container;
    const InsertChainAddress address;
    const int slotIndex;
    const int parameterIndex;
    const float oldValue;
    const float newValue;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UndoableEffectParameterAction)
};

/**
 * Undo step for what the user did in a hosted plugin's own window: the
 * effect's state before and after, handed to the effect through the chain.
 * The plugin holds the new state already when the step is recorded, so
 * performing it changes nothing; undo and redo put the states back.
 */
struct UndoableEffectStateAction final : public juce::UndoableAction
{
    UndoableEffectStateAction (AudioTrackContainer& container_, InsertChainAddress address_, int slotIndex_, json oldState_, json newState_) :
        container (container_),
        address (address_),
        slotIndex (slotIndex_),
        oldState (std::move (oldState_)),
        newState (std::move (newState_))
    {
    }

    bool perform() override { return apply (newState); }
    bool undo() override { return apply (oldState); }
    int getSizeInUnits() override { return 1 + static_cast<int> ((oldState.dump().size() + newState.dump().size()) / 4096); }

private:
    bool apply (const json& state)
    {
        auto* chain = address.resolve (container);
        return chain != nullptr && chain->setState (slotIndex, state);
    }

    AudioTrackContainer& container;
    const InsertChainAddress address;
    const int slotIndex;
    const json oldState;
    const json newState;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UndoableEffectStateAction)
};

} // namespace audium
