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

class TransportSourceProvider;

class AudioResourceContainer {
    
    
public:
    AudioResourceContainer(std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager,
                           std::shared_ptr<TransportSourceProvider> transportSourceProvider);
    
    ~AudioResourceContainer();
    
    std::shared_ptr<AudioResource> addAudioResource (juce::URL resource);
    
    bool removeAudioResource (int atIndex);
    
    int getAudioResourceSize() const { return static_cast<int>(audioResources.size()); }
    
    std::shared_ptr<AudioResource> getAudioResource(int index) const;
    
    /// returns the maximum length of all audio resources
    double getTotalLengthMax() const;
    
    
    bool writeToStream (juce::OutputStream& outputStream);
    bool readFromStream (juce::InputStream& inputStream);
    
    std::shared_ptr<TransportSourceProvider> getTransportSourceProvider() const { return transportSourceProvider; }
    
private:
    
    std::vector<std::shared_ptr<AudioResource>> audioResources;
    
    std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager;
    std::shared_ptr<TransportSourceProvider> transportSourceProvider;
    
    /// TODO: find a proper home for this
    juce::AudioFormatManager formatManager;
        
    /// TODO: find a proper home for this
    juce::TimeSliceThread thread  { "audio file read ahead" };
    
    juce::AudioThumbnailCache thumbnailCache  { 5 };
    
    /// TODO: find a proper home for this
    bool isPlaying = false;
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioResourceContainer)
};
