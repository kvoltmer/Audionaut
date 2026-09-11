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

    /**
     * The playback speed of this object: 2.0 plays the source at double
     * speed (re-pitched one octave up), occupying half the timeline.
     *
     * The invariant used throughout: getRegionData() is always the SOURCE
     * window; the timeline extent is sourceLength / speed. Every helper
     * below converts between the two with this ratio, so overriding it (see
     * PlayListItem, DspClip) makes layout, trims, splits and range queries
     * speed-aware in one place. Regions themselves stay at 1.0.
     */
    virtual double getSpeedRatio() const { return 1.0; }

    juce::Range<double> getAbsolutePositionRange(audium::TimeContextType context) const
    {
        const auto start = getAbsolutePosition(context);
        const auto length = getRegionData(context).getLength() / getSpeedRatio();
        return juce::Range<double>(start, start + length);
    }
    
    // set the left edge of a positionable item; newStart is a timeline
    // position - the source window shrinks/grows by the speed-scaled diff
    void setAbsoluteStartPosition(double newStart, audium::TimeContextType context)
    {
        auto regionData = getRegionData(context);
        
        // offset in file
        auto diff = (newStart - getAbsolutePosition(context)) * getSpeedRatio();
        auto newLength = regionData.getLength() - diff;
        auto newRegionStart = regionData.getStart() + diff;
        setRegionData(juce::Range<double>(newRegionStart, newRegionStart + newLength), context);
        
        setAbsolutePosition(newStart, context);
    }
    
    // set the timeline length of a positionable item (the source window is
    // scaled by the speed)
    void setLength(double newLength, audium::TimeContextType context)
    {
        jassert(newLength >= 0.0);
        auto regionData = getRegionData(context);
        regionData.setLength(newLength * getSpeedRatio());
        setRegionData(regionData, context);
    }
    
    // set the absolute end of a positionable item
    void setAbsoluteEndPosition(double end, audium::TimeContextType context)
    {
        setLength(end - getAbsolutePosition(context), context);
        // the speed scaling round-trips through a multiply/divide
        jassert(std::abs(getAbsolutePositionRange(context).getEnd() - end) <= 1.0e-9);
    }
    
    // move left edge by amount
    void moveAbsoluteStartPosition(double amount, audium::TimeContextType context)
    {
        setAbsoluteStartPosition(getAbsolutePosition(context) + amount, context);
    }
    
    // move timeline length by amount
    void moveLength(double amount, audium::TimeContextType context)
    {
        setLength(getAbsolutePositionRange(context).getLength() + amount, context);
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
