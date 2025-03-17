//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "PositionableBase.h"


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


