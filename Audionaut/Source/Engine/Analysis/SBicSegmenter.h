//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <vector>
#include <JuceHeader.h>

namespace audium {

/**
 * @class SBicSegmenter
 * @brief Segments an audio file into homogeneous regions using Essentia's
 *        SBic (Bayesian Information Criterion) algorithm over MFCC features.
 *
 * The public interface is deliberately free of any Essentia types so this
 * header can be included from engine code (e.g. AnalysisProvider) that does
 * not itself link against Essentia. The signal chain
 * (MonoLoader -> FrameCutter -> Windowing -> Spectrum -> MFCC -> SBic) lives
 * entirely in the .cpp.
 */
class SBicSegmenter {

public:
    /**
     * @brief Tunable parameters for the segmentation. Defaults match the
     *        values validated in the Essentia segmentation test.
     */
    struct Parameters {
        float sampleRate = 44100.0f;
        int   frameSize  = 2048;
        int   hopSize    = 1024;

        // BIC segmentation window/increment sizes, in frames.
        int size1 = 300, inc1 = 60, size2 = 200, inc2 = 20;

        // Minimum segment length, in frames.
        int minimumSegmentLength = 10;

        // Complexity penalty weight [0, inf].
        float complexityPenaltyWeight = 1.5f;
    };

    SBicSegmenter() = default;

    /**
     * @brief Computes segment boundaries using the default parameters.
     * @param audioFile The audio file to analyse.
     * @return Segment boundary timestamps in seconds. Empty on failure.
     */
    std::vector<float> analyze(const juce::File& audioFile);

    /**
     * @brief Computes segment boundaries for the given audio file.
     * @param audioFile The audio file to analyse.
     * @param params Segmentation parameters.
     * @return Segment boundary timestamps in seconds. Empty on failure.
     */
    std::vector<float> analyze(const juce::File& audioFile,
                               const Parameters& params);

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SBicSegmenter)
};

} // namespace audium
