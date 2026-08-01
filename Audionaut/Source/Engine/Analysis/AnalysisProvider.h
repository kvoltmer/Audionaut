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
