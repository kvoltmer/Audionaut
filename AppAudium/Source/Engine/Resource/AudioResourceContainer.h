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

#include "Engine/Resource/AudioResource.h"
#include "Engine/Group/AudioGroup.h"
#include "Engine/Group/AudioSubGroup.h"
#include "Engine/Provider/TempoProvider.h"

class TransportSourceContainer;
class AudioGroupContainer;
class AudiumEngine;

class AudioResourceContainer :  public juce::ActionBroadcaster
{
public:
    AudioResourceContainer(std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager,
                           std::shared_ptr<AudioGroupContainer> audioGroupContainer,
                           std::shared_ptr<juce::AudioFormatManager> formatManager,
                           std::shared_ptr<juce::AudioThumbnailCache> audioThumbnailCache,
                           std::shared_ptr<TempoProvider> tempoProvider) :
        audioDeviceManager(audioDeviceManager),
        audioGroupContainer(audioGroupContainer),
        formatManager(formatManager),
        audioThumbnailCache(audioThumbnailCache),
        tempoProvider(tempoProvider)
    {
        formatManager->registerBasicFormats();
        thread.startThread();
    }
    
    ~AudioResourceContainer();
    
    std::shared_ptr<AudioResource> addAudioResource (juce::URL url,
                                                     std::shared_ptr<AudioGroup> group,
                                                     std::shared_ptr<AudioSubGroup> subGroup,
                                                     int channelPosition = -1,
                                                     int resourceId = -1);
    
    void removeAudioResource(std::shared_ptr<AudioResource> resource);
    void removeAudioResourcesForGroup (std::shared_ptr<AudioGroup> group);
    
    // still used by auto edit
    int getNumAudioResources() const;
    std::shared_ptr<AudioResource> getAudioResource(int index) const;
    std::vector<std::shared_ptr<AudioResource>> resourcesAtAbsolutePosition(double positionInSeconds) const;
    
    int getNumAudioGroups() const;
    std::shared_ptr<AudioGroup> getAudioGroup(int index) const;
    
    void cleanup();
    
    std::vector<std::shared_ptr<AudioResource>> getAudioResourcesForGroup(AudioGroup *group) const;
    std::vector<std::shared_ptr<AudioResource>> getAudioResourcesForSubGroup(const AudioSubGroup *subGroup) const;
    std::vector<std::shared_ptr<AudioResource>> getAudioResourcesForGroupAtAbsoluteRange(AudioGroup *group, juce::Range<double> rangeInSeconds) const;

    std::shared_ptr<AudioGroup> getAudioGroupForResource(std::shared_ptr<AudioResource> resource) const;
    
    std::vector<std::shared_ptr<AudioGroup>> getAudioGroups() const;
        
    void onDeleteChannel(std::shared_ptr<AudioChannel> channel);
    
    void prepareToPlay (double sampleRate, int blockSize);
    
    std::shared_ptr<juce::AudioFormatManager> getAudioFormatManager() const { return formatManager; }
    std::shared_ptr<juce::AudioThumbnailCache> getAudioThumbnailCache() const { return audioThumbnailCache; }
    std::shared_ptr<TempoProvider> getTempoProvider() const { return tempoProvider; }
    
    typedef std::pair<std::shared_ptr<AudioGroup>, std::shared_ptr<AudioResource>> tAudioGroupPair;
    
    void deselectAllResources();
    
    std::shared_ptr<AudioResource> getAudioResourceById(int resourceId) const;
    
    int getNextId() { return ++nextId; }
        
private:
    /// list of pairs. this enables sorting etc.. by AudioGroup
    std::list<tAudioGroupPair> audioResources;
    
    std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager;
    std::shared_ptr<AudioGroupContainer> audioGroupContainer;
    std::shared_ptr<juce::AudioFormatManager> formatManager;
    std::shared_ptr<juce::AudioThumbnailCache> audioThumbnailCache;
    std::shared_ptr<TempoProvider> tempoProvider;
        
    int nextId = 0;
    
    /// TODO: find a proper home for this
    juce::TimeSliceThread thread  { "audio file read ahead" };
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioResourceContainer)
};
