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
 * material's tempo. The segment count is derived today; the length bounds are
 * meant to follow.
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

private:
    static constexpr double beatsPerMeasure = 4.0;

    double measures = 0.0;
};

} // namespace audium
