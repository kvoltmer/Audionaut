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
     * The UI does not offer this: it always runs the built-in analysis.
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

    /// The abstract musical parameter: target segment length in measures
    /// (bars). When positive, numSegments and the segment length bounds are
    /// derived from it and the analysed file's own tempo (see
    /// AutoEditParameter); at zero it is off and the concrete values below are
    /// used as given.
    double segmentMeasures = 0.0;

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
     * @brief Points @p config at the selected playlist item.
     * @return False when no playlist item is selected; @p config is then left
     *         untouched, so a pending edit keeps its current target.
     */
    bool targetSelectedClip(AutoEditConfig &config);

    /**
     * @brief Points @p config at the clip an auto edit invoked without further
     *        targeting applies to.
     *
     * The selected playlist item wins; without one, the first clip of at least
     * a second on the default track is used. Fills trackId / playlistItemId,
     * leaving both at -1 when nothing qualifies.
     */
    void targetSelection(AutoEditConfig &config);

    /**
     * @brief Publishes the boundaries an invokeAutoEdit() with this config
     *        would cut at, as a merge preview on the AnalysisProvider.
     *
     * Views overlay the preview on the clips of the previewed file (see
     * SegmentationView). When the target cannot be resolved or its analyses
     * are not cached yet, any active preview is cleared instead.
     *
     * Published are exactly the cuts invoking the edit would make: the
     * boundaries after the same grid snapping, without the ones falling on or
     * outside the clip's edges (which the merge always emits but the edit
     * drops).
     *
     * Native source only - the Python path computes its boundaries in a
     * subprocess at invoke time, so there is nothing to preview.
     *
     * @return True when a preview was published.
     */
    bool previewAutoEdit(AutoEditConfig &config);

    /**
     * @brief The number of segments an invokeAutoEdit() with this config would
     *        actually create inside the target clip.
     *
     * The merge's segment parameter describes the whole analysed file (see
     * AutoEditConfig::segmentMeasures), but only the boundaries falling inside
     * the clip become cuts. This resolves the parameter, runs the merge, and
     * counts what survives the clip's extent - after the same grid snapping
     * invokeAutoEdit() would apply - the number the UI can show for the
     * pending edit. Zero means no boundary falls inside the clip and the edit
     * would cut nothing.
     *
     * @return The in-clip count; config.numSegments (or the value the measure
     *         parameter derives) when the target does not resolve or its
     *         analyses are not cached yet, since there is nothing to count.
     */
    int resolveNumSegments(AutoEditConfig &config);

    /**
     * @brief The segment length bounds the abstract measure parameter
     *        resolves to for the target file, in seconds.
     *
     * Half the target length below, double above (see AutoEditParameter).
     * The lower bound is what invokeAutoEdit() feeds the merge's
     * minimum-segment rule; the upper bound is advisory until the merge
     * learns one.
     *
     * @return The bounds, or an empty range when the parameter is off, the
     *         target does not resolve, or the file's tempo is not cached.
     */
    juce::Range<double> resolveSegmentLengthBounds(AutoEditConfig &config);

    /**
     * @brief Runs the gaborgandalf subprocess over @p audioFile and turns the
     *        segment file it writes into regions.
     */
    bool invokePython(const juce::File& audioFile,
                      AutoEditConfig &config,
                      std::function<void(std::string)> callback);

    /**
     * @brief Snaps boundaries onto the project's beat grid.
     *
     * Each boundary strictly inside the played region is mapped onto the
     * timeline at the clip's position, moved to the nearest grid beat, and
     * mapped back into file time. A boundary whose nearest beat sits within a
     * beat of the clip's start or end is skipped - the cut would leave a
     * sub-beat sliver of a segment against the clip's edge. Boundaries on or
     * outside the region's edges pass through untouched - snapping them
     * inwards would invent a cut where the analysis put none. Boundaries that
     * snap onto the same beat are collapsed into one, so no zero-length
     * segment can result.
     *
     * Callers gate this on AnalysisProvider::matchesGrid() - snapping is only
     * meaningful when the clip's beat analysis already sits on the grid.
     *
     * @param boundarySeconds     Boundary times, in seconds within the
     *                            analysed file.
     * @param projectTempoBpm     The project tempo.
     * @param clipStartClocks     The clip's absolute timeline position, in
     *                            clocks.
     * @param playedRegionSeconds The part of the file the clip plays, in
     *                            seconds within the file.
     * @return The snapped boundaries, ascending and duplicate-free.
     */
    static std::vector<float> snapBoundariesToGrid(const std::vector<float>& boundarySeconds,
                                                   double projectTempoBpm,
                                                   double clipStartClocks,
                                                   juce::Range<double> playedRegionSeconds);

    /**
     * @brief Drops boundaries within a beat of the clip's start or end.
     *
     * The sub-beat-sliver rule of snapBoundariesToGrid() for clips whose beat
     * analysis does not sit on the grid: cuts are not moved, but one that
     * would leave a segment shorter than a (project-tempo) beat against the
     * clip's edge is skipped. A boundary exactly a beat from an edge is kept,
     * as are boundaries on or outside the edges (they make no cut and are
     * filtered later either way).
     *
     * @param boundarySeconds     Boundary times, in seconds within the
     *                            analysed file.
     * @param projectTempoBpm     The project tempo.
     * @param playedRegionSeconds The part of the file the clip plays, in
     *                            seconds within the file.
     * @return The surviving boundaries.
     */
    static std::vector<float> skipEdgeBoundaries(const std::vector<float>& boundarySeconds,
                                                 double projectTempoBpm,
                                                 juce::Range<double> playedRegionSeconds);

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
     * The clip's own item is removed and the segments are placed where it
     * sat, in order, tiling exactly the stretch it covered - the neighbouring
     * items keep their positions. The clip's region is left alone - it still
     * describes the audio the segments came from, and other items may
     * reference it.
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

