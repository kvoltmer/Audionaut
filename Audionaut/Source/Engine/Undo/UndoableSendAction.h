//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Engine/Effects/AuxBus.h"
#include "Engine/Effects/SendList.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Channel/AudioChannel.h"

namespace audium
{

/**
 * Undo step for the values of one send: level, pre-fader and mute, old and
 * new, applied straight into the send's atomics, so no plan is rebuilt and
 * playback continues undisturbed. Addressed by track, channel and aux id
 * and resolved at apply time, so it stays valid across rebuilds. A send
 * that is not there any more (its aux removed, the channel gone) makes the
 * step a no-op rather than a failure, like the channel actions.
 */
struct UndoableSendAction final : public juce::UndoableAction
{
    struct Values
    {
        float level = 0.0f;
        bool preFader = false;
        bool mute = false;

        static Values of (const SendState& state) noexcept
        {
            return { state.level.load (std::memory_order_relaxed),
                     state.preFader.load (std::memory_order_relaxed),
                     state.mute.load (std::memory_order_relaxed) };
        }

        void applyTo (SendState& state) const noexcept
        {
            state.level.store (juce::jmax (0.0f, level), std::memory_order_relaxed);
            state.preFader.store (preFader, std::memory_order_relaxed);
            state.mute.store (mute, std::memory_order_relaxed);
        }
    };

    UndoableSendAction (AudioTrackContainer& container_, int trackId_, int channelNumber_, int auxId_, Values oldValues_, Values newValues_) noexcept :
        container (container_),
        trackId (trackId_),
        channelNumber (channelNumber_),
        auxId (auxId_),
        oldValues (oldValues_),
        newValues (newValues_)
    {
    }

    bool perform() override { return apply (newValues); }
    bool undo() override { return apply (oldValues); }
    int getSizeInUnits() override { return 1; }

private:
    bool apply (const Values& values)
    {
        auto track = container.getAudioTrack (trackId);
        if (track == nullptr)
            return true;
        auto channel = track->getChannel (channelNumber);
        if (channel == nullptr)
            return true;
        if (auto state = channel->sends.get (auxId))
            values.applyTo (*state);
        return true;
    }

    AudioTrackContainer& container;
    const int trackId;
    const int channelNumber;
    const int auxId;
    const Values oldValues;
    const Values newValues;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UndoableSendAction)
};

/**
 * Undo step for the return values of one aux bus: gain, balance, mute and
 * solo, old and new, into the bus's atomics; no plan is rebuilt. Addressed
 * by aux id; a bus that is gone makes the step a no-op.
 */
struct UndoableAuxAction final : public juce::UndoableAction
{
    struct Values
    {
        float gain = 1.0f;
        float balance = 0.0f;
        bool mute = false;
        bool solo = false;

        static Values of (const AuxBus& bus) noexcept
        {
            return { bus.getGain(), bus.getBalance(), bus.getMute(), bus.getSolo() };
        }

        void applyTo (AuxBus& bus) const noexcept
        {
            bus.setGain (gain);
            bus.setBalance (balance);
            bus.setMute (mute);
            bus.setSolo (solo);
        }
    };

    UndoableAuxAction (AudioTrackContainer& container_, int auxId_, Values oldValues_, Values newValues_) noexcept :
        container (container_),
        auxId (auxId_),
        oldValues (oldValues_),
        newValues (newValues_)
    {
    }

    bool perform() override { return apply (newValues); }
    bool undo() override { return apply (oldValues); }
    int getSizeInUnits() override { return 1; }

private:
    bool apply (const Values& values)
    {
        if (auto bus = container.getAuxBuses().get (auxId))
            values.applyTo (*bus);
        return true;
    }

    AudioTrackContainer& container;
    const int auxId;
    const Values oldValues;
    const Values newValues;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UndoableAuxAction)
};

} // namespace audium
