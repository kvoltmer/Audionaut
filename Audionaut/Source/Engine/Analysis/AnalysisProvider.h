//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Analysis/SBicSegmenter.h"
#include "Engine/Analysis/OnsetSegmenter.h"
#include "Engine/Analysis/AnalysisCache.h"

namespace audium {

class AnalysisProvider {


public:
    AnalysisProvider(AudioTrackContainer &audioTrackContainer_,
                     std::shared_ptr<SBicSegmenter> sBicSegmenter_,
                     std::shared_ptr<OnsetSegmenter> onsetSegmenter_,
                     std::shared_ptr<AnalysisCache> analysisCache_) :
        audioTrackContainer(audioTrackContainer_),
        sBicSegmenter(sBicSegmenter_),
        onsetSegmenter(onsetSegmenter_),
        analysisCache(analysisCache_)
    {}


    void analyzeSBic(AudioTrack& audioTrack);
    void analyzeOnsets(AudioTrack& audioTrack);

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

private:
    // Runs the given segmenter over every audio resource of the track,
    // consulting/updating the cache and storing results under analysisType.
    void analyzeResources(AudioTrack& audioTrack,
                          AnalysisType analysisType,
                          const std::function<std::vector<float>(const juce::File&)>& segmenter);

    AudioTrackContainer &audioTrackContainer;
    std::shared_ptr<SBicSegmenter> sBicSegmenter;
    std::shared_ptr<OnsetSegmenter> onsetSegmenter;
    std::shared_ptr<AnalysisCache> analysisCache;

    // Most recent segment boundaries (seconds), keyed by analysis type and
    // then by analysed file path.
    std::unordered_map<AnalysisType,
                       std::unordered_map<std::string, std::vector<float>>> analysisResults;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnalysisProvider)
};

} // namespace audium
