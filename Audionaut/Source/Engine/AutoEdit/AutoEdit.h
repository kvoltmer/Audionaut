//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <JuceHeader.h>

namespace audium {


class AudiumEngine;
class AudioTrack;
class PlayListContainer;
class AudioTrackContainer;
class PlayListItem;
class ResourceGroup;
class AudioRegion;

struct AutoEditConfig {
    /**
     * @brief Where the segment boundaries come from.
     *
     * The dialog does not offer this: it always runs the built-in analysis.
     * Python is selectable from tests, which is where the two implementations
     * are compared - see AutoEditComparisonTests.
     */
    enum class Source {
        /// AnalysisProvider + EventMerger, merged from the cached analyses.
        Native,
        /// The gaborgandalf Python subprocess. Retained so its boundaries can
        /// be compared against the native ones on the same material; unlike the
        /// native path it needs a working gaborgandalf environment.
        Python
    };

    Source source = Source::Native;

    /// Replace the edited clip in the arrangement with the segments cut from
    /// it. Off leaves the arrangement alone and only adds the regions.
    bool replacePlayListItem = true;

    std::string mode = "random";
    double duration = 120.0;
    int numSegments = 20;
    double minSegLength = 2.0;
    double maxSegLength = 60.0;
    int trackId = -1;
    int playlistItemId = -1;
};

/**
 * @class AutoEdit
 * @brief Cuts a track's audio into regions at automatically chosen boundaries.
 *
 * The native path merges the analyses already held in the `AnalysisCache` -
 * BIC segmentation and Degara beat tracking - through `EventMerger`, which
 * weights rare structural boundaries above frequent beats so the cuts land on
 * beat-aligned segment boundaries.
 *
 * Nothing is rendered and no temporary files are written: the analyses describe
 * the track's source audio file, which is also what the regions are created
 * against, so boundaries and regions share one timeline.
 */
class AutoEdit {

public:
    AutoEdit(std::shared_ptr<AudiumEngine> audiumEngine_) :
        audiumEngine(audiumEngine_)
    {}

    /**
     * @brief Runs an auto edit over the track named by @p config.
     *
     * @param config   What to edit, and which source to take boundaries from.
     * @param callback Invoked with a human-readable reason when the edit cannot
     *                 run - e.g. the track's analyses are not cached yet.
     * @return True when regions were created.
     */
    bool invokeAutoEdit(AutoEditConfig &config,
                        std::function<void(std::string)> callback);

    /**
     * @brief Publishes the boundaries an invokeAutoEdit() with this config
     *        would cut at, as a merge preview on the AnalysisProvider.
     *
     * Views overlay the preview on the clips of the previewed file (see
     * SegmentationView). When the target cannot be resolved or its analyses
     * are not cached yet, any active preview is cleared instead.
     *
     * Native source only - the Python path computes its boundaries in a
     * subprocess at invoke time, so there is nothing to preview.
     *
     * @return True when a preview was published.
     */
    bool previewAutoEdit(AutoEditConfig &config);

    /**
     * @brief Runs the gaborgandalf subprocess over @p audioFile and turns the
     *        segment file it writes into regions.
     */
    bool invokePython(const juce::File& audioFile,
                      AutoEditConfig &config,
                      std::function<void(std::string)> callback);

    /**
     * @brief Creates one region per consecutive pair of boundaries, covering
     *        the given stretch of audio and nothing outside it.
     *
     * Boundaries come from analysing a whole file, but a clip need not span all
     * of it. Those falling outside @p extent are discarded and the survivors
     * bracketed by its edges, so the regions tile exactly what was selected.
     *
     * @param boundarySeconds Ascending boundary times, in seconds.
     * @param extent          The stretch of audio being edited, in seconds.
     * @param track           The track to create the regions on.
     * @param resourceGroup   The resource group the regions belong to.
     * @return True when regions were created; false when no boundary falls
     *         inside @p extent, leaving nothing to cut.
     */
    std::vector<std::shared_ptr<AudioRegion>>
        createRegionsFromBoundaries(const std::vector<float>& boundarySeconds,
                                    juce::Range<double> extent,
                                    std::shared_ptr<AudioTrack> track,
                                    std::shared_ptr<ResourceGroup> resourceGroup);

    /**
     * @brief Puts the segments into the arrangement in place of the clip they
     *        were cut from.
     *
     * The segments are inserted where the clip sat, in order, and the clip's
     * own item is then removed. Its region is left alone - it still describes
     * the audio the segments came from, and other items may reference it.
     *
     * @param track              The track whose arrangement to rewrite.
     * @param playListItemIndex  The clip to replace.
     * @param regions            The segments, in playing order.
     * @return True when the arrangement was rewritten.
     */
    bool replacePlayListItemWithRegions(std::shared_ptr<AudioTrack> track,
                                        int playListItemIndex,
                                        const std::vector<std::shared_ptr<AudioRegion>>& regions);

    /** @brief Reads the Python path's segment file and creates regions from it. */
    bool createRegionsFromSegFile(std::string segFileName,
                                  double sampleRate,
                                  std::shared_ptr<AudioTrack> track,
                                  std::shared_ptr<ResourceGroup> resourceGroup);

    static const juce::String getTempDirectory();

private:
    std::shared_ptr<AudiumEngine> audiumEngine;

    std::string audioResourceFilePath;

    const std::string getBaseName() const;

    // Wraps region creation in the undo transaction the UI expects, so one
    // Undo removes every region an edit produced.
    bool applyAsUndoableEdit(std::function<bool()> createRegions);
};

} // namespace audium

