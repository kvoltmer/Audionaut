//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <string>
#include <vector>
#include <JuceHeader.h>

namespace audium {

/**
 * @class EventMerger
 * @brief Merges several independent event streams (segment boundaries, beat
 *        grids) into a single set of cut points.
 *
 * This is a C++ port of the "layer 6" event merge from the gaborgandalf Python
 * package (`segments.py`, `compute_event_merge_combined`), the step that
 * reconciles the outputs of the individual analyses into the boundaries an
 * automatic edit actually cuts on.
 *
 * The algorithm superimposes a Ricker ("mexican hat") kernel on every event,
 * with a width derived from that stream's mean event interval, sums the
 * resulting activations across all streams, and keeps the strongest peaks.
 * Because the kernel width scales with the interval, rarer events (segment
 * boundaries) outweigh frequent ones (beats), so the final cuts land on
 * beat-aligned segment boundaries.
 *
 * Unlike the segmenters in this directory this class needs no Essentia - it is
 * pure arithmetic over `std::vector<float>` - so it builds and is tested
 * unconditionally. Do not introduce an Essentia dependency here.
 *
 * The public interface speaks seconds, matching SBicSegmenter, OnsetSegmenter,
 * BeatSegmenter and AnalysisProvider. Internally the merge runs on a discrete
 * frame grid (see Parameters::hopSize / Parameters::gridRate), whose defaults
 * reproduce the librosa grid the reference implementation used.
 */
class EventMerger {

public:
    /**
     * @brief What an event stream represents, which selects how it is treated
     *        when the activation matrix is built.
     */
    enum class Kind {
        Segmentation, ///< Structural boundaries (e.g. SBicSegmenter output).
        Beat          ///< A beat grid (e.g. BeatSegmenter output).
    };

    /**
     * @brief One stream of events to merge.
     */
    struct EventStream {
        std::string label;              ///< Diagnostic only, e.g. "sbic", "beats_120".
        Kind kind = Kind::Segmentation; ///< How the stream is treated.
        std::vector<float> times;       ///< Event times in seconds, ascending.
    };

    /**
     * @brief Tunable parameters for the merge.
     *
     * The defaults reproduce the behaviour of the Python reference, including
     * its quirks (see the flags below), so results can be compared against it
     * directly.
     */
    struct Parameters {
        /** @name Analysis grid */
        ///@{
        /// Frame hop, in samples, of the internal grid. librosa's default.
        int hopSize = 512;
        /// Sample rate the grid is expressed in. The reference analysed at this
        /// rate; it need not match the rate the audio was segmented at.
        float gridRate = 22050.0f;
        ///@}

        /** @name Algorithm */
        ///@{
        /// How many activation peaks to keep (`numsegs` in the reference).
        int numSegments = 20;
        /// Upper bound on the kernel length, in frames. The kernel is shortened
        /// further when the material is shorter than this.
        int maxKernelPoints = 250;
        /// A stream's mean interval is divided by this to give its Ricker width.
        float kernelWidthDivisor = 10.0f;
        /// Boundaries closer together than this are collapsed. The reference
        /// hardcoded 43, which is ~0.998 s on the default grid.
        int minSegmentFrames = 43;
        ///@}

        /**
         * @name Reference quirks
         *
         * Each of these reproduces a behaviour of the Python implementation
         * that looks unintended. They default to the reference behaviour so
         * output stays comparable; flip them to get what the code appears to
         * have meant. See docs/plans/eventmerger-layer6-port/plan.md.
         */
        ///@{
        /// The reference computes per-stream kernel weights and then never
        /// applies them, summing the activations unweighted. When true, stream
        /// `i` is scaled by `1 + i` as that dead code intended.
        bool applyKernelWeights = false;

        /// The reference shifts every peak index by an unexplained -2 before
        /// the minimum-length pass. Set to 0 to disable the shift.
        int peakIndexOffset = -2;

        /// The reference indexes segmentation streams as `segs[i][:-1]`,
        /// silently discarding each stream's final boundary. Beat streams are
        /// not truncated. When false, all boundaries are kept.
        bool dropLastSegBoundary = true;

        /// The reference pairs the activation column of the i-th *valid* beat
        /// stream with the events of the i-th beat stream overall, so a stream
        /// with too few events to yield an interval shifts the pairing. The two
        /// agree whenever every beat stream is valid. When true, columns are
        /// paired with the stream their interval came from.
        bool strictStreamMapping = false;
        ///@}
    };

    /**
     * @brief Result of a merge.
     */
    struct Result {
        /// Merged cut points in seconds, ascending. Empty on failure or when
        /// the inputs are degenerate.
        std::vector<float> boundaries;

        /// The summed per-frame activation the boundaries were picked from,
        /// one entry per frame. Retained for debugging and visualisation;
        /// empty whenever `boundaries` is.
        std::vector<float> activation;
    };

    EventMerger() = default;

    /**
     * @brief Merges the given streams using the default parameters.
     * @param streams          The event streams to merge.
     * @param durationSeconds  Length of the analysed material, in seconds.
     * @return The merged boundaries. Empty on degenerate input.
     */
    Result merge(const std::vector<EventStream>& streams,
                 float durationSeconds);

    /**
     * @brief Merges the given streams.
     * @param streams          The event streams to merge.
     * @param durationSeconds  Length of the analysed material, in seconds.
     * @param params           Merge parameters.
     * @return The merged boundaries. Empty on degenerate input: no streams, a
     *         non-positive duration or fewer than one requested segment.
     */
    Result merge(const std::vector<EventStream>& streams,
                 float durationSeconds,
                 const Parameters& params);

    /** @name Grid conversion
     *
     * Conversions between the public seconds domain and the internal frame
     * grid. Exposed so callers can relate a Result back to the grid it was
     * computed on. These are pure conversions and do not clamp: a negative or
     * past-the-end time maps to a correspondingly out-of-range frame, and it is
     * the caller's job to discard it.
     */
    ///@{
    /**
     * @brief Quantises a time to its grid frame, rounding to nearest.
     *
     * Rounding (rather than truncating) makes the conversion round-trip stable,
     * so a frame index turned into seconds and back yields the same frame.
     *
     * @return The frame index, or 0 if the parameters describe no usable grid.
     */
    static int timeToFrame(float seconds, const Parameters& params);

    /**
     * @brief The time, in seconds, at which a grid frame starts.
     * @return The time, or 0 if the parameters describe no usable grid.
     */
    static float frameToTime(int frame, const Parameters& params);

    /**
     * @brief How many whole grid frames the given duration spans.
     *
     * Truncates, matching librosa's `samples_to_frames`, so this is the frame
     * count the reference implementation worked with.
     *
     * @return The frame count, or 0 for a non-positive duration or no usable
     *         grid.
     */
    static int frameCount(float durationSeconds, const Parameters& params);
    ///@}

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EventMerger)
};

} // namespace audium
