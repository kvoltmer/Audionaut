/*
  ==============================================================================

    SnapToGridHandler.h
    Created: 25 Mar 2024 3:54:41pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class SnapToGridHandler : public juce::ChangeBroadcaster {
    
    
public:
    void publishRange(juce::Range<double> clocks);
    
    void clearRange();
    
    juce::Range<double> getRange() const { return clockRange; }
    
private:
    
    juce::Range<double> clockRange;
    
};
