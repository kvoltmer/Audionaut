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
#include "AudioResourceGroup.h"

class TransportSourceContainer;

class AudioResourceContainer : public juce::ActionBroadcaster
{
public:
    AudioResourceContainer(std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager,
                           std::shared_ptr<TransportSourceContainer> transportSourceContainer);
    
    ~AudioResourceContainer();
    
    std::shared_ptr<AudioResource> addAudioResource (juce::URL resource, std::shared_ptr<AudioResourceGroup> group = nullptr);
    
    bool removeAudioResourceGroup (int atIndex);
    
    // obsolete?
    int getNumAudioResources() const;
    
    int getNumAudioResourceGroups() const;
    
    // obsolete?
    std::shared_ptr<AudioResource> getAudioResource(int index) const;
    
    std::shared_ptr<AudioResourceGroup> getAudioResourceGroup(int index) const;
    
    /// returns the maximum length of all audio resources
    double getTotalLengthMax() const;
    
    
    bool writeToStream (juce::OutputStream& outputStream);
    bool readFromStream (juce::InputStream& inputStream);
    
    std::shared_ptr<TransportSourceContainer> getTransportSourceContainer() const { return transportSourceContainer; }
    
    void cleanup() { audioResources.clear(); }
    
    std::vector<std::shared_ptr<AudioResource>> getAudioResourcesForGroup(std::shared_ptr<AudioResourceGroup> group) const;
    
    std::vector<std::shared_ptr<AudioResourceGroup>> getAudioResourceGroups() const;
    
    typedef std::pair<std::shared_ptr<AudioResourceGroup>, std::shared_ptr<AudioResource>> tAudioGroupPair;
    
private:
    /// list of pairs. this enables sorting etc.. by AudioResourceGroup
    std::list<tAudioGroupPair> audioResources;
    
    std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager;
    std::shared_ptr<TransportSourceContainer> transportSourceContainer;
    
    /// TODO: find a proper home for this
    juce::AudioFormatManager formatManager;
        
    /// TODO: find a proper home for this
    juce::TimeSliceThread thread  { "audio file read ahead" };
    
    juce::AudioThumbnailCache thumbnailCache  { 64 };
    
    /// TODO: find a proper home for this
    bool isPlaying = false;
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioResourceContainer)
};
