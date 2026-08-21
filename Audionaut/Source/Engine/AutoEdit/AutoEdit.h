//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <functional>
#include <memory>
#include <random>
#include <string>
#include <vector>
#include <JuceHeader.h>

#include "Engine/Analysis/AnalysisCache.h"

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

    /// Apply a symmetric crossfade at every joint between the segments
    /// (half the length on each side of the cut).
    bool crossfades = true;
    double crossfadeSeconds = 0.02;

    /// The fade curve exponent at the joints: 0.5 = equal power (the
    /// ClipDynamics default), 1.0 = linear (transparent for the contiguous
    /// material a split produces).
    double crossfadeCurve = 0.5;

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
 * @brief How a track's regions are assembled into a new playlist.
 *
 * The C++ port of gaborgandalf's final layer: the segments the auto edit cut
 * become the source material a song of the wanted duration is assembled from.
 */
struct AssembleConfig {
    enum class Mode {
        /// Segments are drawn uniformly at random, with replacement, until the
        /// song is long enough - so segments may repeat or be left out.
        Random,
        /// Segments keep their order but are thinned out with probability
        /// duration / total length, walking the material again from the start
        /// until the song is long enough - so shorter material repeats.
        Sequential
    };

    Mode mode = Mode::Random;

    /// Target song length, in seconds. Random mode overshoots it by up to one
    /// segment; sequential mode hits it only in expectation.
    double duration = 60.0;

    int trackId = -1;

    /// Seeds the draws, so a fixed seed reproduces the same song. The default
    /// mirrors gaborgandalf's config; interactive callers pass a random seed
    /// to get a fresh arrangement each time.
    unsigned int seed = 1234;
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
     * @brief Assembles a new playlist for the track from its regions.
     *
     * The C++ port of gaborgandalf's assemble step: the track's regions -
     * typically the segments a previous auto edit cut - are the source
     * material, and the arrangement is rebuilt as a song of roughly
     * config.duration seconds. Every playlist item on the track is removed
     * first (the regions all stay), then one item per chosen region is placed
     * butt-joined from the start of the timeline, in the order the mode chose.
     * One Undo restores the previous arrangement.
     *
     * @param config   Mode, target duration, track and seed.
     * @param callback Invoked with a human-readable reason when nothing was
     *                 assembled - e.g. the track has no regions yet.
     * @return True when the playlist was rebuilt.
     */
    bool invokeAssemble(AssembleConfig &config,
                        std::function<void(std::string)> callback);

    /**
     * @brief Points @p config at the track an assemble applies to.
     *
     * The selected clip's track wins; without one, the default track. Leaves
     * trackId at -1 when neither exists.
     */
    void targetAssembleTrack(AssembleConfig &config);

    /**
     * @brief The playing order of AssembleConfig::Mode::Random, as indices
     *        into @p lengthsSeconds.
     *
     * Indices are drawn uniformly at random with replacement and appended
     * until the accumulated length reaches @p targetSeconds, so the last pick
     * may overshoot it by up to one segment (gaborgandalf's
     * track_assemble_from_segments).
     *
     * @param lengthsSeconds Segment lengths, all positive - the caller filters.
     * @param targetSeconds  The wanted song length.
     * @param rng            The seeded generator the picks are drawn from.
     * @return The chosen indices in playing order; empty when there is nothing
     *         to choose from or the target is not positive.
     */
    static std::vector<int> chooseRandomSequence(const std::vector<double>& lengthsSeconds,
                                                 double targetSeconds,
                                                 std::mt19937& rng);

    /**
     * @brief The playing order of AssembleConfig::Mode::Sequential, as indices
     *        into @p lengthsSeconds.
     *
     * The input order is kept but thinned out: the segments are visited in
     * order, wrapping around at the end, and each visit survives with
     * gaborgandalf's thinning probability targetSeconds / total length
     * (track_assemble_from_segments_sequential_scale) - continuing until the
     * accumulated length reaches the target, so the last keep may overshoot it
     * by up to one segment. A target no shorter than the total keeps
     * everything, repeating the material as often as it takes. One draw is
     * spent per visited segment either way, so a given seed always produces
     * the same keep-or-skip pattern.
     *
     * @param lengthsSeconds Segment lengths, all positive - the caller filters.
     * @param targetSeconds  The wanted song length.
     * @param rng            The seeded generator the draws come from.
     * @return The kept indices in playing order; empty when there was nothing
     *         to choose from, or when an absurdly small target exhausts the
     *         internal visit cap before anything is kept.
     */
    static std::vector<int> chooseSequentialSequence(const std::vector<double>& lengthsSeconds,
                                                     double targetSeconds,
                                                     std::mt19937& rng);

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
     * @brief Whether every analysis the edit's merge needs is cached for the
     *        target's audio file.
     *
     * False while the AnalysisWorker is still busy on the file - the usual
     * reason previewAutoEdit() has nothing to show yet - which lets callers
     * say "still analysing" instead of a vague failure. A target that does not
     * resolve has no analyses to wait for and reports true; whatever else is
     * wrong is not going to fix itself by waiting.
     */
    bool isAnalysisDone(AutoEditConfig &config);

    /**
     * @brief The merge analyses the target's file is missing that are switched
     *        off in the auto-analysis settings, so waiting will never supply
     *        them.
     *
     * Reads the AnalysisWorker's live settings: with automatic analysis
     * disabled every missing merge analysis is reported; otherwise only the
     * missing ones excluded from the worker's default types. An analysis that
     * is switched off but already cached is not reported - its results are
     * there regardless. Empty when nothing is missing, or when the target does
     * not resolve.
     *
     * Callers use this to tell "still analysing, try again shortly" apart
     * from "switched off in the settings, enable it there".
     */
    std::vector<AnalysisType> findSwitchedOffMergeAnalyses(AutoEditConfig &config);

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
     * @param baseName        What the segments are named after - the edited
     *                        clip's name; each region becomes
     *                        <baseName>-seg-<number> through the container's
     *                        unique-name mechanism.
     * @return True when regions were created; false when no boundary falls
     *         inside @p extent, leaving nothing to cut.
     */
    std::vector<std::shared_ptr<AudioRegion>>
        createRegionsFromBoundaries(const std::vector<float>& boundarySeconds,
                                    juce::Range<double> extent,
                                    std::shared_ptr<AudioTrack> track,
                                    std::shared_ptr<ResourceGroup> resourceGroup,
                                    const juce::String& baseName);

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
                                        const std::vector<std::shared_ptr<AudioRegion>>& regions,
                                        double crossfadeSeconds = 0.0,
                                        double crossfadeCurve = 0.5);

    /** @brief Reads the Python path's segment file and creates regions from
     *         it, named <baseName>-seg-<number> like the native path's. */
    bool createRegionsFromSegFile(std::string segFileName,
                                  double sampleRate,
                                  std::shared_ptr<AudioTrack> track,
                                  std::shared_ptr<ResourceGroup> resourceGroup,
                                  const juce::String& baseName);

    static const juce::String getTempDirectory();

private:
    std::shared_ptr<AudiumEngine> audiumEngine;

    std::string audioResourceFilePath;

    const std::string getBaseName() const;

    // Wraps a mutation in the undo transaction the UI expects, so one Undo
    // takes back everything the edit changed.
    bool applyAsUndoableEdit(std::function<bool()> mutate,
                             const juce::String& transactionName = "Auto Edit");
};

} // namespace audium

