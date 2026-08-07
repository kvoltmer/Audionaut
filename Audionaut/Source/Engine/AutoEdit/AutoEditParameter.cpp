//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <algorithm>
#include <cmath>

#include "AutoEditParameter.h"

namespace audium {

double AutoEditParameter::measureSeconds(double bpm)
{
    if (bpm <= 0.0)
        return 0.0;

    return beatsPerMeasure * 60.0 / bpm;
}

int AutoEditParameter::numSegmentsFor(double durationSeconds, double bpm) const
{
    if (! isActive() || durationSeconds <= 0.0)
        return 0;

    const auto segmentSeconds = measures * measureSeconds(bpm);

    if (segmentSeconds <= 0.0)
        return 0;

    return std::max(1, (int) std::lround(durationSeconds / segmentSeconds));
}

double AutoEditParameter::minSegmentSeconds(double bpm) const
{
    if (! isActive())
        return 0.0;

    return minSegmentMeasures() * measureSeconds(bpm);
}

double AutoEditParameter::maxSegmentSeconds(double bpm) const
{
    if (! isActive())
        return 0.0;

    return maxSegmentMeasures() * measureSeconds(bpm);
}

} // namespace audium
