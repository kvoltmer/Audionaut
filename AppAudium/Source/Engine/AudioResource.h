/*
  ==============================================================================

    AudioResource.h
    Created: 29 Jan 2023 12:55:52pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>




class AudioResource {
    
public:
    AudioResource(juce::URL resource, juce::AudioFormatManager& formatManager);
    
private:
    juce::URL resource;
    
    /// TODO: capsulate?
    juce::AudioThumbnailCache thumbnailCache  { 5 };
    
public:
    
    juce::AudioThumbnail thumbnail;
};
