//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <atomic>
#include <vector>
#include <JuceHeader.h>

namespace audium {

/**
 * @class OnsetSegmenter
 * @brief Detects note/percussive onsets in an audio file using Essentia's
 *        OnsetRate algorithm.
 *
 * As with SBicSegmenter, the public interface is deliberately free of any
 * Essentia types so this header can be included from engine code (e.g.
 * AnalysisProvider) that does not itself link against Essentia. The signal
 * chain (MonoLoader -> OnsetRate) lives entirely in the .cpp.
 */
class OnsetSegmenter {

public:
    /**
     * @brief Tunable parameters for the onset detection.
     */
    struct Parameters {
        float sampleRate = 44100.0f;
    };

    OnsetSegmenter() = default;

    /**
     * @brief Detects onsets using the default parameters.
     * @param audioFile The audio file to analyse.
     * @return Onset timestamps in seconds. Empty on failure.
     */
    std::vector<float> analyze(const juce::File& audioFile);

    /**
     * @brief Detects onsets for the given audio file.
     * @param audioFile The audio file to analyse.
     * @param params Onset-detection parameters.
     * @param shouldAbort Optional flag polled between the analysis stages
     *        (decoding, onset detection). Once set, the analysis gives up at
     *        the next poll and returns empty, so a caller shutting down (see
     *        AnalysisWorker) waits for one stage at most rather than for the
     *        whole file.
     * @return Onset timestamps in seconds. Empty on failure or when aborted.
     */
    std::vector<float> analyze(const juce::File& audioFile,
                               const Parameters& params,
                               const std::atomic<bool>* shouldAbort = nullptr);

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OnsetSegmenter)
};

} // namespace audium
