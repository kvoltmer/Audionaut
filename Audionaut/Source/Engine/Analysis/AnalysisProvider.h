//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Analysis/SBicSegmenter.h"
#include "Engine/Analysis/OnsetSegmenter.h"
#include "Engine/Analysis/BeatSegmenter.h"
#include "Engine/Analysis/AnalysisCache.h"
#include "Engine/Analysis/EventMerger.h"

namespace audium {

/**
 * @class AnalysisProvider
 * @brief Runs/caches audio analyses and broadcasts a change whenever one
 *        finishes, so UI views (e.g. AudioClipView) can refresh without
 *        polling. One shared instance serves the whole engine (see
 *        AudiumFactory), so listeners don't need to know which track/file
 *        triggered the change - they simply re-read the cache for whatever
 *        they display.
 */
class AnalysisProvider : public juce::ChangeBroadcaster {


public:
    AnalysisProvider(std::shared_ptr<SBicSegmenter> sBicSegmenter_,
                     std::shared_ptr<OnsetSegmenter> onsetSegmenter_,
                     std::shared_ptr<BeatSegmenter> beatSegmenter_,
                     std::shared_ptr<AnalysisCache> analysisCache_) :
        sBicSegmenter(sBicSegmenter_),
        onsetSegmenter(onsetSegmenter_),
        beatSegmenter(beatSegmenter_),
        analysisCache(analysisCache_)
    {}


    void analyzeSBic(AudioTrack& audioTrack);
    void analyzeOnsets(AudioTrack& audioTrack);
    void analyzeBeats(AudioTrack& audioTrack);
    void analyzeBeatsDegara(AudioTrack& audioTrack);

    /**
     * @brief Analyses a single audio file for one analysis type, consulting and
     *        updating the shared cache.
     *
     * This is the single-file entry point used by the background
     * `AnalysisWorker`. It is safe to call from a non-message thread: the cache
     * does its own locking and segmenter access is serialised internally, so a
     * background analysis may run concurrently with a UI-triggered one.
     *
     * @param audioFile     The audio file to analyse.
     * @param analysisType  The kind of analysis to run.
     * @return The segment boundaries (seconds), from cache or freshly computed;
     *         empty on failure.
     */
    std::vector<float> analyzeFile(const juce::File& audioFile,
                                   AnalysisType analysisType);

    /**
     * @brief Segment boundaries (in seconds) from the most recent analysis of
     *        the given type for the given file.
     * @return The boundaries, or an empty vector if the file has not been
     *         analysed with that type.
     */
    std::vector<float> getSegments(AnalysisType analysisType,
                                   const juce::File& audioFile) const;

    /**
     * @brief All results for an analysis type, keyed by analysed file path.
     * @return The per-file results, or an empty map if nothing has been
     *         analysed with that type.
     */
    std::unordered_map<std::string, std::vector<float>>
        getSegments(AnalysisType analysisType) const;

    /**
     * @brief BPM estimate from the most recent beat-tracking analysis of the
     *        given type for the given file.
     * @return The BPM estimate, or 0.0f if the file has not been analysed
     *         with that type.
     */
    float getBpm(AnalysisType analysisType, const juce::File& audioFile) const;

    /** @brief The underlying (persistent) analysis cache. */
    std::shared_ptr<AnalysisCache> getCache() const { return analysisCache; }

    /** @name Merged analysis
     *
     * Combines several analyses of one file into a single set of boundaries,
     * via `EventMerger`. Because a wider kernel spreads more energy, the rarer
     * structural boundaries dominate the frequent beats, so the result lands on
     * beat-aligned segment boundaries rather than on either analysis alone.
     *
     * getMergeAnalysisTypes() is the authority on which analyses take part;
     * onsets did once and no longer do.
     */
    ///@{
    /** @brief The analyses the merge is built from, in stream order. */
    static const std::vector<AnalysisType>& getMergeAnalysisTypes();

    /**
     * @brief Assembles analysis results into the merge's input streams.
     *
     * BIC boundaries become the (rare, structural) segmentation stream and
     * beats the event stream. An analysis with no results contributes no stream
     * at all, rather than an empty one.
     *
     * Pure and free of any Essentia dependency, so it can be exercised without
     * running an analysis.
     *
     * @param sBicSegments Boundaries from AnalysisType::SBic, in seconds.
     * @param degaraBeats  Beats from AnalysisType::BeatDegara, in seconds.
     */
    static std::vector<EventMerger::EventStream>
        makeMergeStreams(const std::vector<float>& sBicSegments,
                         const std::vector<float>& degaraBeats);

    /**
     * @brief Which of the merge's analyses are not yet cached for a file.
     *
     * Lets a caller explain what is missing instead of silently producing no
     * boundaries. Empty means the file is ready to merge.
     */
    std::vector<AnalysisType> findMissingMergeAnalyses(const juce::File& audioFile) const;

    /**
     * @brief Merges the cached analyses of a file into segment boundaries.
     *
     * Reads results from the cache only - it never runs an analysis, so it is
     * safe to call from the message thread. `AnalysisWorker` normally populates
     * the cache when the file is added, long before this is called.
     *
     * @param audioFile       The analysed audio file.
     * @param durationSeconds Length of the file, in seconds.
     * @param params          Merge parameters.
     * @return The merged boundaries, or an empty result if any of the analyses
     *         is missing - see findMissingMergeAnalyses().
     */
    EventMerger::Result mergeCachedAnalyses(const juce::File& audioFile,
                                            float durationSeconds,
                                            const EventMerger::Parameters& params) const;

    /** @brief Merges the cached analyses using the default merge parameters. */
    EventMerger::Result mergeCachedAnalyses(const juce::File& audioFile,
                                            float durationSeconds) const;
    ///@}

private:
    // Analyses every audio resource of the track for the given type, delegating
    // each file to analyzeFile().
    void analyzeTrack(AudioTrack& audioTrack, AnalysisType analysisType);

    // Result of a single segmenter run: segment boundaries plus (for
    // beat-tracking types) the overall BPM estimate.
    struct SegmentResult {
        std::vector<float> segments;
        float bpm = 0.0f;
    };

    // Dispatches to the segmenter matching the analysis type.
    SegmentResult runSegmenter(AnalysisType analysisType,
                               const juce::File& audioFile);

    std::shared_ptr<SBicSegmenter> sBicSegmenter;
    std::shared_ptr<OnsetSegmenter> onsetSegmenter;
    std::shared_ptr<BeatSegmenter> beatSegmenter;
    std::shared_ptr<AnalysisCache> analysisCache;

    // Serialises segmenter access so a background (AnalysisWorker) analysis and
    // a UI-triggered one never run the same Essentia algorithms concurrently.
    std::mutex segmenterMutex;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnalysisProvider)
};

} // namespace audium
