/*
  ==============================================================================

    AudioResource.h
    Created: 29 Jan 2023 12:55:52pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>


class AudioResourceContainer;

class AudioResource {
    
public:
    AudioResource(AudioResourceContainer& audioResourceContainer,
                  juce::URL resource,
                  juce::AudioFormatManager& formatManager);
    ~AudioResource();
    
    juce::AudioThumbnail& getThumbnail() { return thumbnail; }
    
    /// returns the maximum length of all audio resources
    double getTotalLengthMax() const;

private:

    AudioResourceContainer& audioResourceContainer;
    
    juce::URL resource;
    
    /// TODO: maybe capsulate?
    juce::AudioThumbnailCache thumbnailCache  { 5 };
    juce::AudioThumbnail thumbnail;
    
private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioResource)
};
