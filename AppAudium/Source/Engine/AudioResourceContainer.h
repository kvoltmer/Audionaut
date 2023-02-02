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
    
private:
    
    std::vector<std::shared_ptr<AudioResource>> audioResources;
    
    juce::AudioFormatManager formatManager;
};
