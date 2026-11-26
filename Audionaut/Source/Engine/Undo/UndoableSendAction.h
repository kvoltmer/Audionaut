//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Engine/ActionMessages.h"
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
 *
 * A send the step itself brought into being (the mixer shows every aux as
 * a knob at silence; the first touch makes the entry) is flagged
 * `createdByThisStep`: redo makes the entry again and undo removes it, so
 * a project keeps no silent leftovers.
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

        bool operator== (const Values& other) const noexcept
        {
            return juce::exactlyEqual (level, other.level) && preFader == other.preFader && mute == other.mute;
        }
        bool operator!= (const Values& other) const noexcept { return ! (*this == other); }
    };

    UndoableSendAction (AudioTrackContainer& container_, int trackId_, int channelNumber_, int auxId_,
                        Values oldValues_, Values newValues_, bool createdByThisStep_ = false) noexcept :
        container (container_),
        trackId (trackId_),
        channelNumber (channelNumber_),
        auxId (auxId_),
        oldValues (oldValues_),
        newValues (newValues_),
        createdByThisStep (createdByThisStep_)
    {
    }

    bool perform() override { return apply (newValues, createdByThisStep ? Entry::make : Entry::keep); }
    bool undo() override { return apply (oldValues, createdByThisStep ? Entry::remove : Entry::keep); }
    int getSizeInUnits() override { return 1; }

private:
    enum class Entry { keep, make, remove };

    bool apply (const Values& values, Entry entry)
    {
        auto track = container.getAudioTrack (trackId);
        if (track == nullptr)
            return true;
        auto channel = track->getChannel (channelNumber);
        if (channel == nullptr)
            return true;

        if (entry == Entry::remove)
            channel->sends.remove (auxId);
        else if (auto state = entry == Entry::make ? channel->sends.getOrAdd (auxId) : channel->sends.get (auxId))
            values.applyTo (*state);

        container.sendActionMessage (updateChannelsAction);   // the strips re-read the send (undo and redo included)
        return true;
    }

    AudioTrackContainer& container;
    const int trackId;
    const int channelNumber;
    const int auxId;
    const Values oldValues;
    const Values newValues;
    const bool createdByThisStep;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UndoableSendAction)
};

/**
 * Undo step for the return values of one aux bus: gain, pan, mute and
 * solo, old and new, into the bus's atomics; no plan is rebuilt. Addressed
 * by aux id; a bus that is gone makes the step a no-op.
 */
struct UndoableAuxAction final : public juce::UndoableAction
{
    struct Values
    {
        float gain = 1.0f;
        float pan = 0.0f;
        bool mute = false;
        bool solo = false;

        static Values of (const AuxBus& bus) noexcept
        {
            return { bus.getGain(), bus.getPan(), bus.getMute(), bus.getSolo() };
        }

        void applyTo (AuxBus& bus) const noexcept
        {
            bus.setGain (gain);
            bus.setPan (pan);
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
        container.sendActionMessage (updateChannelsAction);   // the strips re-read the bus (undo and redo included)
        return true;
    }

    AudioTrackContainer& container;
    const int auxId;
    const Values oldValues;
    const Values newValues;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UndoableAuxAction)
};

/**
 * Undo step for the master's values: gain, pan (balance) and mute, before
 * and after a fader or pan drag or a mute click (the mixer's master strip,
 * the header's fader), applied straight to the container, which hands them
 * to the renderer without a new plan.
 */
struct UndoableMasterAction final : public juce::UndoableAction
{
    struct Values
    {
        float gain = 1.0f;
        float pan = 0.0f;
        bool mute = false;

        static Values of (const AudioTrackContainer& container) noexcept
        {
            return { container.getMasterGain(), container.getMasterPan(), container.getMasterMute() };
        }

        void applyTo (AudioTrackContainer& container) const
        {
            container.setMasterGain (gain);
            container.setMasterPan (pan);
            container.setMasterMute (mute);
        }

        bool operator== (const Values& other) const noexcept
        {
            return juce::exactlyEqual (gain, other.gain) && juce::exactlyEqual (pan, other.pan) && mute == other.mute;
        }
        bool operator!= (const Values& other) const noexcept { return ! (*this == other); }
    };

    UndoableMasterAction (AudioTrackContainer& container_, Values oldValues_, Values newValues_) noexcept :
        container (container_),
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
        values.applyTo (container);
        container.sendActionMessage (updateChannelsAction);   // the master strip re-reads them; the header follows on its timer
        return true;
    }

    AudioTrackContainer& container;
    const Values oldValues;
    const Values newValues;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UndoableMasterAction)
};

} // namespace audium
