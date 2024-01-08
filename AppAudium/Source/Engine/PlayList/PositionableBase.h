/*
  ==============================================================================

    PositionableBase.h
    Created: 7 Jan 2024 11:41:31am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Engine/TimeContext.h"

class PositionableBase
{
    
protected:
    PositionableBase() = default;
    virtual ~PositionableBase() = default;
    
public:
    
    virtual juce::Range<double> getAbsolutePosition(audium::TimeContextType context) const = 0;

private:
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PositionableBase)
};
