//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Lola uses a GPL/commercial licence - see LICENCE.md for details.

#include "PositionableBase.h"

namespace audium {

const juce::Range<double> PositionableBase::absoluteToLocalRange(const juce::Range<double> absoluteRange,
                                                                 audium::TimeContextType context) const
{
    auto start = absoluteToLocalPosition(absoluteRange.getStart(), context);
    auto end = absoluteToLocalPosition(absoluteRange.getEnd(), context);
    return juce::Range<double>(start, end);
}

const double PositionableBase::absoluteToLocalPosition(const double absolutePosition,
                                                       audium::TimeContextType context) const
{
    return absolutePosition - getAbsolutePosition(context) + getRegionData(context).getStart();
}

} // namespace audium


