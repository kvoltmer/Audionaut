/*
  ==============================================================================

    AudioRegion.h
    Created: 30 May 2023 10:16:15am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>

class AudioGroup;

class AudioRegion
{    
    
public:
    AudioRegion(std::shared_ptr<AudioGroup> audioGroup) :
        audioGroup(audioGroup)
    {
        jassert(audioGroup != nullptr);
    }
    
    std::shared_ptr<AudioGroup> getAudioGroup() const { return audioGroup; }
    
    typedef class juce::Range<double> RegionData;
    
    RegionData position;
    
    juce::String name;
    
private:
    
    std::shared_ptr<AudioGroup> audioGroup;
    
    JUCE_LEAK_DETECTOR (AudioRegion)
};
