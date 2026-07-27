//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <vector>
#include <JuceHeader.h>

namespace audium {

/**
 * @class BeatDegaraSegmenter
 * @brief Tracks beat positions in an audio file using Essentia's
 *        BeatTrackerDegara algorithm (streaming mode).
 *
 * BeatTrackerDegara trades some accuracy for significantly higher throughput
 * compared to BeatTrackerMultiFeature (see BeatSegmenter), which makes it
 * suitable for batch-processing large numbers of files.
 *
 * As with the other segmenters, the public interface is free of any Essentia
 * types so this header can be included from engine code (e.g.
 * AnalysisProvider) that does not itself link against Essentia. The streaming
 * network (MonoLoader -> BeatTrackerDegara) lives entirely in the .cpp.
 */
class BeatDegaraSegmenter {

public:
    /**
     * @brief Tunable parameters for the beat tracking.
     */
    struct Parameters {
        float sampleRate = 44100.0f;
    };

    BeatDegaraSegmenter() = default;

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
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BeatDegaraSegmenter)
};

} // namespace audium
