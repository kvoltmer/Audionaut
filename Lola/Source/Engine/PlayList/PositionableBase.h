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

#pragma once

#include <JuceHeader.h>
#include "Engine/TimeContext.h"

namespace audium {

class PositionableBase
{
    
protected:
    PositionableBase() = default;
    virtual ~PositionableBase() = default;
    
public:
    
    virtual juce::Range<double> getRegionData(audium::TimeContextType context) const = 0;
    virtual void setRegionData(juce::Range<double> newRegionData, audium::TimeContextType context) = 0;
    
    virtual double getAbsolutePosition(audium::TimeContextType context) const = 0;
    virtual void setAbsolutePosition(double position, audium::TimeContextType context) = 0;
    
    juce::Range<double> getAbsolutePositionRange(audium::TimeContextType context) const
    {
        const auto start = getAbsolutePosition(context);
        const auto length = getRegionData(context).getLength();
        return juce::Range<double>(start, start + length);
    }
    
    // set the left edge of a positionalbe item
    void setAbsoluteStartPosition(double newStart, audium::TimeContextType context)
    {
        auto regionData = getRegionData(context);
        
        // offset in file
        auto diff = newStart - getAbsolutePosition(context);
        auto newLength = regionData.getLength() - diff;
        auto newRegionStart = regionData.getStart() + diff;
        setRegionData(juce::Range<double>(newRegionStart, newRegionStart + newLength), context);
        
        setAbsolutePosition(newStart, context);
    }
    
    // set the length of a positionalbe item
    void setLength(double newLength, audium::TimeContextType context)
    {
        auto regionData = getRegionData(context);
        regionData.setLength(newLength);
        setRegionData(regionData, context);
    }
    
    // move left edge by amount
    void moveAbsoluteStartPosition(double amount, audium::TimeContextType context)
    {
        setAbsoluteStartPosition(getAbsolutePosition(context) + amount, context);
    }
    
    // move length by amount
    void moveLength(double amount, audium::TimeContextType context)
    {
        setLength(getRegionData(context).getLength() + amount, context);
    }
    
    // move position by amount
    void moveAbsolutePosition(double amount, audium::TimeContextType context)
    {
        setAbsolutePosition(getAbsolutePosition(context) + amount, context);
    }
    
    const juce::Range<double> absoluteToLocalRange(const juce::Range<double> absoluteRange,
                                                   audium::TimeContextType context) const;
    
    const double absoluteToLocalPosition(const double absolutePosition,
                                         audium::TimeContextType context) const;
    
private:
    
    JUCE_LEAK_DETECTOR (PositionableBase)
};

} // namespace audium 
