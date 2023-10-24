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
#include "AudioGroup.h"

class TransportSourceContainer;
class AudioGroupContainer;
class AudiumEngine;

class AudioResourceContainer : public juce::ActionBroadcaster
{
public:
    AudioResourceContainer(std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager,
                           std::shared_ptr<AudioGroupContainer> audioGroupContainer);
    
    ~AudioResourceContainer();
    
    std::shared_ptr<AudioResource> addAudioResource (juce::URL url,
                                                     const AudiumEngine &engine,
                                                     std::shared_ptr<AudioGroup> group = nullptr);
    
    void removeAudioResource(std::shared_ptr<AudiumEngine> engine, std::shared_ptr<AudioResource> resource);
    void removeAudioResourcesForGroup (std::shared_ptr<AudioGroup> group);
    
    // still used by auto edit
    int getNumAudioResources() const;
    std::shared_ptr<AudioResource> getAudioResource(int index) const;
    
    int getNumAudioGroups() const;
    std::shared_ptr<AudioGroup> getAudioGroup(int index) const;
    
    /// returns the maximum length of all audio resources
    double getTotalLengthMax() const;
    
    
    bool writeToStream (juce::OutputStream& outputStream);
    bool readFromStream (juce::InputStream& inputStream, const AudiumEngine& engine);
    
    void cleanup() { audioResources.clear(); }
    
    std::vector<std::shared_ptr<AudioResource>> getAudioResourcesForGroup(AudioGroup *group) const;
    
    std::shared_ptr<AudioGroup> getAudioGroupForResource(std::shared_ptr<AudioResource> resource) const;
    
    std::vector<std::shared_ptr<AudioGroup>> getAudioGroups() const;
    
    std::shared_ptr<AudioGroup> getDefaultGroup() const;
    
    int getNumChannels() const;
    std::shared_ptr<AudioResource> getChannel(int index) const;
    
    typedef std::pair<std::shared_ptr<AudioGroup>, std::shared_ptr<AudioResource>> tAudioGroupPair;
    
private:
    /// list of pairs. this enables sorting etc.. by AudioGroup
    std::list<tAudioGroupPair> audioResources;
    
    std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager;
    std::shared_ptr<AudioGroupContainer> audioGroupContainer;
    
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
