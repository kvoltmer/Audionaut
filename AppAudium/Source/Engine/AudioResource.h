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
    
    juce::AudioThumbnail& getThumbnail() { return thumbnail; }
    
private:
    juce::URL resource;
    
    /// TODO: maybe capsulate?
    juce::AudioThumbnailCache thumbnailCache  { 5 };
    juce::AudioThumbnail thumbnail;
    
private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioResource)
};
