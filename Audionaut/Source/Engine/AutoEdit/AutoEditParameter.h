//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

namespace audium {

/**
 * @class AutoEditParameter
 * @brief An abstract, musical control over the concrete auto edit parameters.
 *
 * The concrete parameters the merge consumes (segment count, length bounds)
 * are not how an edit is thought about: material is cut in measures, not in
 * segment counts. This class holds the abstract value - a target segment
 * length in measures (bars) - and derives the concrete values from it and the
 * material's tempo: the segment count, and the segment length bounds that
 * bracket the target musically.
 *
 * A measure is four beats, the project-wide convention (see TempoProvider,
 * where a 96-clock bar is four 24-clock beats). The tempo is expected to be
 * the analysed file's own, from its cached beat analysis, rather than the
 * project tempo - so a measure spans what a measure of that audio actually
 * lasts.
 *
 * A value of zero or less means the parameter is off and the concrete values
 * rule; see AutoEditConfig::segmentMeasures.
 */
class AutoEditParameter {

public:
    explicit AutoEditParameter(double measures_) : measures(measures_) {}

    /** @brief Whether the parameter takes part in deriving anything at all. */
    bool isActive() const { return measures > 0.0; }

    /** @brief The abstract value: target segment length, in measures. */
    double getMeasures() const { return measures; }

    /**
     * @brief Seconds one measure spans at the given tempo.
     * @return The measure length, or 0 when @p bpm is not positive.
     */
    static double measureSeconds(double bpm);

    /**
     * @brief The segment count that cuts material of the given length into
     *        segments of this parameter's measure length.
     *
     * Rounded to nearest, and never below one: material shorter than a single
     * segment still yields one rather than none.
     *
     * @param durationSeconds Length of the analysed material, in seconds.
     * @param bpm             The material's tempo.
     * @return The derived count, or 0 when nothing can be derived - the
     *         parameter is off, or the duration or tempo is not positive.
     */
    int numSegmentsFor(double durationSeconds, double bpm) const;

    /** @name Segment length bounds
     *
     * The bounds bracket the target length musically: half the target below,
     * double the target above. A segment may run a little short or long of
     * the target, but not leave its musical order of magnitude.
     *
     * The minimum feeds the merge's minimum-segment rule (see
     * EventMerger::Parameters::minSegmentFrames); the maximum is advisory
     * until the merge learns an upper bound.
     */
    ///@{
    /** @brief The shortest segment the edit should produce, in measures. */
    double minSegmentMeasures() const { return measures * minRatio; }

    /** @brief The longest segment the edit should produce, in measures. */
    double maxSegmentMeasures() const { return measures * maxRatio; }

    /**
     * @brief The shortest segment the edit should produce, in seconds.
     * @param bpm The material's tempo.
     * @return The bound, or 0 when nothing can be derived - the parameter is
     *         off or the tempo is not positive.
     */
    double minSegmentSeconds(double bpm) const;

    /**
     * @brief The longest segment the edit should produce, in seconds.
     * @param bpm The material's tempo.
     * @return The bound, or 0 when nothing can be derived - the parameter is
     *         off or the tempo is not positive.
     */
    double maxSegmentSeconds(double bpm) const;
    ///@}

private:
    static constexpr double beatsPerMeasure = 4.0;

    // How far a segment may fall short of or overshoot the target length.
    static constexpr double minRatio = 0.5;
    static constexpr double maxRatio = 2.0;

    double measures = 0.0;
};

} // namespace audium
