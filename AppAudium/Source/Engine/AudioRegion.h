/*
  ==============================================================================

    AudioRegion.h
    Created: 30 May 2023 10:16:15am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>

class AudioRegion
{    
    
public:
    AudioRegion() = default;
    
    juce::Range<double> position;
    
    juce::String name;
    
private:
    JUCE_LEAK_DETECTOR (AudioRegion)
};
