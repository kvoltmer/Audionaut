//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <vector>
#include <JuceHeader.h>

namespace audium {

/**
 * @class BeatSegmenter
 * @brief Tracks beat positions in an audio file using Essentia's
 *        BeatTrackerMultiFeature algorithm (streaming mode).
 *
 * As with SBicSegmenter and OnsetSegmenter, the public interface is free of
 * any Essentia types so this header can be included from engine code (e.g.
 * AnalysisProvider) that does not itself link against Essentia. The streaming
 * network (MonoLoader -> BeatTrackerMultiFeature) lives entirely in the .cpp.
 */
class BeatSegmenter {

public:
    /**
     * @brief Beat-tracking method, passed through to RhythmExtractor2013's
     *        "method" parameter.
     */
    enum class Method {
        MultiFeature,
        Degara
    };

    /**
     * @brief Tunable parameters for the beat tracking.
     */
    struct Parameters {
        float sampleRate = 44100.0f;
        Method method = Method::MultiFeature;
        // RhythmExtractor2013's own tempo search range defaults.
        int maxTempo = 208;
        int minTempo = 40;
    };

    BeatSegmenter() = default;

    /**
     * @brief Tracks beats using the default parameters.
     * @param audioFile The audio file to analyse.
     * @return Beat timestamps in seconds. Empty on failure.
     */
    std::vector<float> analyze(const juce::File& audioFile);

    /**
     * @brief Tracks beats for the given audio file.
     * @param audioFile The audio file to analyse.
     * @param params Beat-tracking parameters.
     * @return Beat timestamps in seconds. Empty on failure.
     */
    std::vector<float> analyze(const juce::File& audioFile,
                               const Parameters& params);

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BeatSegmenter)
};

} // namespace audium
