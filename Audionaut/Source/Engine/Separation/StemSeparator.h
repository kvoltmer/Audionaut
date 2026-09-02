//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <array>
#include <memory>
#include <vector>
#include <JuceHeader.h>

#include "Engine/Separation/SeparationBackend.h"
#include "Engine/Separation/SeparationTypes.h"

namespace audium {

class AudiumEngine;
struct ExportAudioConfig;

/// Which clip to separate and how.
struct SeparationConfig
{
    /// The track's index in the container (AudioTrack::getId()).
    int trackId = -1;

    /// The clip's index on that track (PlayListItem::getId()); -1 for the
    /// track's first clip.
    int playlistItemId = -1;

    /// Segments to separate in parallel; physical cores is the useful
    /// maximum.
    int numThreads = 1;

    /// Longer clips are refused: the separator holds the whole clip and
    /// all its stems in memory (about 85 MB per minute).
    double maxClipSeconds = 600.0;

    /// Mute the source track when the stems are added, so what plays
    /// afterwards is the separation. Part of the same undo.
    bool muteSourceTrack = true;
};

/// A separation resolved against the project, ready to render. Built on the
/// message thread so the rendering thread never has to look at the project.
struct SeparationJob
{
    std::shared_ptr<ExportAudioConfig> exportConfig;

    /// Scratch directory for this job; removed by commit() and discard().
    juce::File directory;

    /// Where the stems go: the clip's start, less any fade-in extension the
    /// render includes ahead of it.
    double positionClocks = 0.0;

    /// The names of the stem tracks and files derive from this.
    juce::String clipName;

    int sourceTrackId = -1;
    int sourceClipId = -1;
    int numThreads = 1;
    bool muteSourceTrack = true;
};

/// Rendered stems waiting to be added to the project.
struct PendingStems
{
    juce::File directory;
    std::array<juce::File, numStems> files;
    double positionClocks = 0.0;
    juce::String clipName;
    int sourceTrackId = -1;
    int sourceClipId = -1;
    bool muteSourceTrack = true;
};

/**
 * Separates a clip into stems and adds them to the project as new tracks.
 *
 * The work is split into three steps so a GUI can run the slow one off the
 * message thread:
 *
 *   1. prepare()  - message thread: resolve the clip, set up the render.
 *   2. render()   - any thread, transport stopped: bounce the clip at the
 *                   backend's rate, separate, write one WAV per stem.
 *   3. commit()   - message thread: import the stems as tracks, one undo.
 *
 * separate() runs all three back to back for headless callers. Nothing
 * about the backend is assumed - see SeparationBackend.
 */
class StemSeparator
{
public:
    StemSeparator (std::shared_ptr<AudiumEngine> engine,
                   std::shared_ptr<SeparationBackend> backend);

    /// Points @p config at the selected clip. False when no clip is selected.
    bool targetSelectedClip (SeparationConfig& config) const;

    /// Whether @p config can be separated right now; @p reason says why not.
    bool canSeparate (const SeparationConfig& config, juce::String& reason) const;

    /// Step 1. Fails with @p error when canSeparate() would.
    bool prepare (const SeparationConfig& config, SeparationJob& job, juce::String& error) const;

    /**
     * Step 2. Blocks for minutes with the real backend.
     *
     * Renders through the play list scheduler, which must not be playing
     * meanwhile - the caller keeps the transport stopped for the duration,
     * as it does for an export.
     *
     * @return true with @p stems filled; false with @p error set, or with an
     *         empty error when @p progress asked to cancel. The job's scratch
     *         directory is removed on failure.
     */
    bool render (const SeparationJob& job,
                 const SeparationProgress& progress,
                 PendingStems& stems,
                 juce::String& error);

    /**
     * Step 3. Adds one track per stem, named "<clip> - <Stem>", at the
     * source clip's position - muting the source track's channels when the
     * stems ask for it - as a single "Separate Stems" undo transaction,
     * then removes the scratch directory.
     *
     * @param newTrackIds  Receives the ids of the tracks created, in Stem
     *                     order.
     */
    bool commit (const PendingStems& stems, std::vector<int>& newTrackIds, juce::String& error);

    /// All three steps.
    bool separate (const SeparationConfig& config,
                   const SeparationProgress& progress,
                   std::vector<int>& newTrackIds,
                   juce::String& error);

    /// Throws away rendered stems that will not be committed.
    static void discard (const PendingStems& stems);

    /// The name given to a stem's track and file.
    static juce::String stemTrackName (const juce::String& clipName, Stem stem);

private:
    std::shared_ptr<AudiumEngine> audiumEngine;
    std::shared_ptr<SeparationBackend> backend;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StemSeparator)
};

} // namespace audium
