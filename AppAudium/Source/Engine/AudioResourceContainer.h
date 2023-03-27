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
    
    void initializeAudioDevice();
    
    std::shared_ptr<AudioResource> addAudioResource (juce::URL resource);
    
    int getAudioResourceSize() const { return (int)audioResources.size(); }
    
    std::shared_ptr<AudioResource> getAudioResource(int index) { return audioResources[(std::size_t)index]; }
    
    /// returns the maximum length of all audio resources
    double getTotalLengthMax() const;
    
    void start();
    
    void stop();
    
    void playStop();
    
private:
    
    std::vector<std::shared_ptr<AudioResource>> audioResources;
    
    /// TODO: find a proper home for this
    juce::AudioFormatManager formatManager;
    
    /// TODO: find a proper home for this
    juce::AudioDeviceManager audioDeviceManager;
    
    /// TODO: find a proper home for this
    juce::TimeSliceThread thread  { "audio file read ahead" };
    
    juce::AudioThumbnailCache thumbnailCache  { 5 };
    
    /// TODO: find a proper home for this
    bool isPlaying = false;
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioResourceContainer)
};
