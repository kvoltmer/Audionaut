//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include "Engine/ActionMessages.h"
#include "Engine/Channel/AudioChannel.h"
#include "Engine/Channel/AudioChannelData.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"

namespace audium
{

/**
 * @struct UndoableChannelAction
 * @brief Undo step for one channel's mixer parameters (gain, pan, mute,
 *        solo, monitor).
 *
 * Snapshots only the channel's `AudioChannelData` instead of the whole
 * track container: applying it writes the data back into the channel and
 * commits it to the audio bus, so no clip, region or voice source is
 * touched and playback continues undisturbed.
 *
 * The channel is addressed by track id and channel number and resolved at
 * apply time, so the step stays valid across track rebuilds that replace
 * the channel objects.
 */
struct UndoableChannelAction final : public juce::UndoableAction
{
    UndoableChannelAction (AudioTrackContainer& container_, int trackId_, int channelNumber_) noexcept :
        container (container_),
        trackId (trackId_),
        channelNumber (channelNumber_)
    {
        oldData = currentData();
    }

    /** Captures the channel's current state as the state perform() applies. */
    void storeNewState()
    {
        newData = currentData();
    }

    /** True when the stored states are identical - nothing to undo. */
    bool isNoOp() const noexcept
    {
        return oldData == newData;
    }

    bool perform() override
    {
        return apply (newData);
    }

    bool undo() override
    {
        return apply (oldData);
    }

    int getSizeInUnits() override
    {
        return 1;
    }

private:
    std::shared_ptr<AudioChannel> channel() const
    {
        if (auto track = container.getAudioTrack (trackId))
            return track->getChannel (channelNumber);

        return nullptr;
    }

    AudioChannelData currentData() const
    {
        if (auto ch = channel())
            return ch->data;

        return AudioChannelData();
    }

    bool apply (const AudioChannelData& data)
    {
        auto ch = channel();
        if (ch == nullptr)
            return false;

        ch->data = data;
        ch->commitChannelData();

        // solo state changes the mute buttons of every other strip
        container.sendActionMessage (updateChannelsAction);
        return true;
    }

    AudioTrackContainer& container;
    const int trackId;
    const int channelNumber;
    AudioChannelData oldData;
    AudioChannelData newData;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UndoableChannelAction)
};

} // namespace audium
