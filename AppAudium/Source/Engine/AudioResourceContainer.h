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
    void addAudioResource (juce::URL resource);
    
private:
    
    std::vector<std::unique_ptr<AudioResource>> audioResources;
    
};
