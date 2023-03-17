/*
  ==============================================================================

    AudioResourceContainer.h
    Created: 29 Jan 2023 12:37:15pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once
#include <vector>
#include <memory>
#include <JuceHeader.h>
#include "AudioResource.h"

class AudioResourceContainer {
    
    
public:
    AudioResourceContainer();
    
    std::shared_ptr<AudioResource> addAudioResource (juce::URL resource);
    
    int getAudioResourceSize() const { return (int)audioResources.size(); }
    
    std::shared_ptr<AudioResource> getAudioResource(int index) { return audioResources[index]; }
    
    /// returns the maximum length of all audio resources
    double getTotalLengthMax() const;
    
private:
    
    std::vector<std::shared_ptr<AudioResource>> audioResources;
    
    juce::AudioFormatManager formatManager;
    
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioResourceContainer)
};
