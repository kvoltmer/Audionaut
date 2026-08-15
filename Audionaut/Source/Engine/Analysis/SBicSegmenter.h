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
     * @brief Tunable parameters for the segmentation.
     *
     * The BIC values follow gaborgandalf rather than Essentia's stock defaults,
     * which is what this used to carry. Measured against the reference on the
     * project's test material, they are what account for most of the difference
     * in where the two implementations cut: the lower complexity penalty in
     * particular roughly doubles the number of boundaries found, while the
     * feature the BIC runs over - MFCC here, chroma there - turns out to matter
     * comparatively little.
     *
     * Note the frame rate works out the same on both sides despite the
     * different rate and hop: 44100/1024 and 22050/512 are both 43.07 frames
     * per second, so the window and length values transfer directly.
     */
    struct Parameters {
        float sampleRate = 44100.0f;
        int   frameSize  = 2048;
        int   hopSize    = 1024;

        // BIC segmentation window/increment sizes, in frames.
        int size1 = 200, inc1 = 60, size2 = 300, inc2 = 20;

        // Minimum segment length, in frames. 50 is ~1.16 s at the frame rate
        // above.
        int minimumSegmentLength = 50;

        // Complexity penalty weight [0, inf]. Lower yields more boundaries.
        float complexityPenaltyWeight = 0.5f;
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
