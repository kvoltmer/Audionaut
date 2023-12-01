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
                           std::shared_ptr<AudioGroupContainer> audioGroupContainer,
                           std::shared_ptr<juce::AudioFormatManager> formatManager,
                           std::shared_ptr<juce::AudioThumbnailCache> audioThumbnailCache) :
        audioDeviceManager(audioDeviceManager),
        audioGroupContainer(audioGroupContainer),
        formatManager(formatManager),
        audioThumbnailCache(audioThumbnailCache)
    {
        formatManager->registerBasicFormats();
        thread.startThread();
    }
    
    ~AudioResourceContainer();
    
    std::shared_ptr<AudioResource> addAudioResource (juce::URL url,
                                                     const AudiumEngine &engine,
                                                     std::shared_ptr<AudioGroup> group);
    
    void removeAudioResource(std::shared_ptr<AudiumEngine> engine, std::shared_ptr<AudioResource> resource);
    void removeAudioResourcesForGroup (std::shared_ptr<AudioGroup> group);
    
    // still used by auto edit
    int getNumAudioResources() const;
    std::shared_ptr<AudioResource> getAudioResource(int index) const;
    std::vector<std::shared_ptr<AudioResource>> resourcesAtAbsolutePosition(double positionInSeconds) const;
    
    int getNumAudioGroups() const;
    std::shared_ptr<AudioGroup> getAudioGroup(int index) const;
    

    bool writeToStream (juce::OutputStream& outputStream);
    bool readFromStream (juce::InputStream& inputStream, const AudiumEngine& engine);
    
    void cleanup() { audioResources.clear(); }
    
    std::vector<std::shared_ptr<AudioResource>> getAudioResourcesForGroup(AudioGroup *group) const;
    
    std::shared_ptr<AudioGroup> getAudioGroupForResource(std::shared_ptr<AudioResource> resource) const;
    
    std::vector<std::shared_ptr<AudioGroup>> getAudioGroups() const;
        
    int getNumChannels() const;
    std::shared_ptr<AudioResource> getChannel(int index) const;
    
    void prepareToPlay (double sampleRate, int blockSize);
    
    std::shared_ptr<juce::AudioFormatManager> getAudioFormatManager() const { return formatManager; }
    std::shared_ptr<juce::AudioThumbnailCache> getAudioThumbnailCache() const { return audioThumbnailCache; }
    
    typedef std::pair<std::shared_ptr<AudioGroup>, std::shared_ptr<AudioResource>> tAudioGroupPair;
    
private:
    /// list of pairs. this enables sorting etc.. by AudioGroup
    std::list<tAudioGroupPair> audioResources;
    
    std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager;
    std::shared_ptr<AudioGroupContainer> audioGroupContainer;
    std::shared_ptr<juce::AudioFormatManager> formatManager;
    std::shared_ptr<juce::AudioThumbnailCache> audioThumbnailCache;
        
    /// TODO: find a proper home for this
    juce::TimeSliceThread thread  { "audio file read ahead" };
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioResourceContainer)
};
