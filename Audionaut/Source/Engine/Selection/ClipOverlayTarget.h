//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

namespace audium {

/**
 * Which clip currently shows an in-arrangement overlay editor (the stretch
 * overlay). Message-thread session state - never persisted.
 *
 * Deliberately its own ChangeBroadcaster: the track container's broadcaster
 * belongs to the scheduler (a change there recommits the audio-thread clip
 * data), and its action broadcaster drives full UI refreshes. Every
 * PlayListItemComponent listens here and self-selects by id, the same
 * pattern the Auto Edit preview uses on the AnalysisProvider.
 */
class ClipOverlayTarget : public juce::ChangeBroadcaster
{
public:
    // the non-copyable macro declares a (deleted) copy constructor, which
    // suppresses the implicit default one
    ClipOverlayTarget() = default;

    void set(int newTrackId, int newPlaylistItemId)
    {
        if (trackId == newTrackId && playlistItemId == newPlaylistItemId)
            return;

        trackId = newTrackId;
        playlistItemId = newPlaylistItemId;
        sendChangeMessage();
    }

    void clear()
    {
        if (! isActive())
            return;

        trackId = -1;
        playlistItemId = -1;
        sendChangeMessage();
    }

    bool isActive() const noexcept { return trackId >= 0 && playlistItemId >= 0; }

    bool matches(int otherTrackId, int otherPlaylistItemId) const noexcept
    {
        return isActive() && trackId == otherTrackId && playlistItemId == otherPlaylistItemId;
    }

    int getTrackId() const noexcept { return trackId; }
    int getPlaylistItemId() const noexcept { return playlistItemId; }

private:
    int trackId = -1;
    int playlistItemId = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClipOverlayTarget)
};

} // namespace audium
