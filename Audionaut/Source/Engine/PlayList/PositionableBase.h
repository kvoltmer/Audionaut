//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include "Engine/TimeContext.h"

namespace audium {

/**
 * \class PositionableBase
 * \brief Base class for objects that can be positioned and resized in a time-based context.
 *
 * This class provides an interface for managing the position and size of objects
 * in a time-based context. It includes methods for retrieving and modifying
 * position and size, as well as utility functions for range conversions.
 */
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
