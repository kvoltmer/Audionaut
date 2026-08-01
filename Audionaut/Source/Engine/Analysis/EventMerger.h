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

    /**
     * @brief The merge's first stage: one activation column per contributing
     *        stream, together with the mean event interval that sizes that
     *        column's kernel.
     *
     * A stream contributes a column only if it holds at least two events, since
     * a single event has no interval to derive a kernel width from. Streams are
     * ordered segmentation-first, then beats, matching the reference.
     */
    struct Activations {
        /// Length of every column, in frames.
        int numFrames = 0;

        /// Mean interval between events, in frames, one per column. Sizes the
        /// column's kernel in the next stage.
        std::vector<float> intervals;

        /// The activation columns: `1` on a frame carrying an event, `0`
        /// elsewhere. `columns[i]` has `numFrames` entries.
        std::vector<std::vector<float>> columns;

        /// Label of the stream each column's events came from. Diagnostics
        /// only - but see Parameters::strictStreamMapping, where this is the
        /// visible difference between the two mappings.
        std::vector<std::string> labels;
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

    /**
     * @brief Runs the merge's first stage on its own.
     *
     * Exposed so the stage can be tested and inspected independently of the
     * kernel pass that consumes it; `merge()` calls it internally.
     *
     * Events are quantised onto the frame grid and those falling outside
     * `[0, numFrames)` are dropped. Intervals are computed from every event of
     * a stream, including any the activation pass then discards - see
     * Parameters::dropLastSegBoundary.
     *
     * @param streams    The event streams to merge.
     * @param numFrames  Length of the material in grid frames, from frameCount().
     * @param params     Merge parameters.
     * @return The activation columns and their intervals. Empty when no stream
     *         carries enough events, or when `numFrames` is below 1.
     */
    static Activations buildActivations(const std::vector<EventStream>& streams,
                                        int numFrames,
                                        const Parameters& params);

    /**
     * @brief Collapses the activation columns into the single per-frame
     *        activation the boundaries are picked from.
     *
     * Each column is convolved with a Ricker kernel whose width comes from that
     * column's mean event interval, and the results are summed. Because a wider
     * kernel spreads more energy, streams with rarer events - segment
     * boundaries - end up dominating streams with frequent ones - beats.
     *
     * A column whose interval is not positive describes no usable kernel width
     * and contributes nothing rather than poisoning the sum.
     *
     * @return One value per frame, or empty if there is nothing to sum.
     */
    static std::vector<float> summedActivation(const Activations& activations,
                                               const Parameters& params);

    /**
     * @brief Picks the frames carrying the strongest activation.
     *
     * Ties are broken by frame order, so the result is deterministic. The
     * reference relies on `np.argpartition`, whose tie ordering is unspecified,
     * which is why exact index parity with it is not guaranteed on ties.
     *
     * Parameters::peakIndexOffset is applied to every index, so the result can
     * contain indices outside the grid; the minimum-length pass discards them.
     *
     * @param activation   Per-frame activation from summedActivation().
     * @param numSegments  How many peaks to keep; clamped to the frame count.
     * @return The picked frame indices in ascending order.
     */
    static std::vector<int> pickPeaks(const std::vector<float>& activation,
                                      int numSegments,
                                      const Parameters& params);

    /**
     * @brief Drops boundaries that would make a segment shorter than
     *        Parameters::minSegmentFrames, and opens the run at frame zero.
     *
     * Note the reference's rule, reproduced here: a peak is measured against
     * the peak *before it in the picked list*, not against the last boundary
     * kept. A run of peaks packed closer than the minimum therefore collapses
     * to whichever of them follows a large enough gap, rather than to the first
     * of the run.
     *
     * The picked peaks are bracketed by frame zero and the last frame before
     * the rule is applied, so the material's end can itself become a boundary.
     * Indices outside the grid - which Parameters::peakIndexOffset can produce
     * - fail the rule and fall away here.
     *
     * @param peaks      Picked frame indices; sorted internally.
     * @param numFrames  Length of the material in grid frames.
     * @return The surviving frame indices, ascending, always starting at zero.
     */
    static std::vector<int> applyMinimumLength(const std::vector<int>& peaks,
                                               int numFrames,
                                               const Parameters& params);

    /** @name Kernel primitives
     *
     * The building blocks of the pass above, exposed so their conventions can
     * be pinned by tests.
     */
    ///@{
    /**
     * @brief A Ricker ("mexican hat") wavelet, matching `scipy.signal.ricker`.
     *
     * That function was removed in SciPy 1.15, so this is the closed form it
     * used:
     * `A (1 - x^2/a^2) exp(-x^2 / 2a^2)`, with `A = 2 / (sqrt(3a) pi^(1/4))`
     * and `x` running symmetrically about the centre point.
     *
     * @param points Kernel length in samples.
     * @param width  The wavelet's `a` parameter. Must be positive.
     * @return The kernel, or empty for a non-positive length or width.
     */
    static std::vector<float> ricker(int points, float width);

    /**
     * @brief Convolution truncated to the signal's length, matching
     *        `np.convolve(..., mode='same')`.
     *
     * The result is the centre `max(M, N)` samples of the full convolution,
     * taken from offset `(K - 1) / 2` where `K` is the shorter input's length.
     * Getting that offset wrong shifts every boundary, so it is pinned by test.
     *
     * @return `max(signal.size(), kernel.size())` samples; empty if either
     *         input is.
     */
    static std::vector<float> convolveSame(const std::vector<float>& signal,
                                           const std::vector<float>& kernel);
    ///@}

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
