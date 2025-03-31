//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Lola uses a GPL/commercial licence - see LICENCE.md for details.

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
