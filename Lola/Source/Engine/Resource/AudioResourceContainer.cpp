/*
  ==============================================================================

 AudioResourceContainer.cpp
    Created: 29 Jan 2023 12:37:15pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/AudioSources/TransportSourceContainer.h"
#include "Engine/AudioSources/AudiumTransportSource.h"
#include "Engine/ActionMessages.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Factory/AudioResourceFactory.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/Group/AudioClip.h"
#include "Engine/Resource/ChannelMapping.h"
#include "Engine/Channel/AudioChannel.h"

AudioResourceContainer::~AudioResourceContainer()
{
    audioResources.clear();
}

std::shared_ptr<AudioResource> AudioResourceContainer::findResourceWithUrl(juce::URL url) const
{
    for (auto it = audioResources.begin(); it != audioResources.end(); ++it) {
        if ((*it).second->getUrl() == url) {
//            std::cout << "found url: " << url.getFileName() << std::endl;
            return (*it).second;
        }
    }
    return nullptr;
}

std::shared_ptr<juce::AudioFormatReader> AudioResourceContainer::getAudioFormatReaderForUrl(juce::URL url)
{
    auto audioFormat = formatManager->findFormatForFileExtension(url.getLocalFile().getFileExtension());
    
    if (audioFormat != nullptr) {
        
        if (auto existingResource = findResourceWithUrl(url)) {
            return existingResource->audioFormatReader;
        }
        else {
            return AudioResourceFactory::createAudioFormatReader(url, *formatManager.get());
        }
    }
    return nullptr;
}


std::shared_ptr<AudioResource> AudioResourceContainer::addAudioResource (juce::URL url,
                                                                         std::shared_ptr<juce::AudioFormatReader> audioFormatReader,
                                                                         std::shared_ptr<AudioTrack> track,
                                                                         std::shared_ptr<AudioSubGroup> subGroup,
                                                                         int destChannel,
                                                                         int sourceChannel)
{
    jassert(track != nullptr);
    jassert(subGroup != nullptr);
            
    auto audioResource = AudioResourceFactory::createAudioResource(url,
                                                                   audioFormatReader,
                                                                   *this,
                                                                   track,
                                                                   subGroup,
                                                                   destChannel,
                                                                   sourceChannel);
    if (audioResource->audioFormatReader != nullptr) {
        audioResources.push_back({track, audioResource});
        return audioResource;
    }
    
    return nullptr;
}

std::shared_ptr<AudiumTransportSource> AudioResourceContainer::createTransportSourceForAudioResource(std::shared_ptr<AudioResource> audioResource)
{
    auto source = std::make_shared<AudioFormatReaderSource>(audioResource->audioFormatReader.get(), false);
    if (source != nullptr && source->getAudioFormatReader() != nullptr) {
        return audioResource->createNewTransportSource(source);
    }
    return nullptr;
}

void AudioResourceContainer::removeAudioResource(std::shared_ptr<AudioResource> resource)
{
    for (auto it = audioResources.begin(); it != audioResources.end(); ++it) {
        if ((*it).second == resource) {
            auto sources = transportSourceContainer->getTransportSourcesForResource(*resource.get());
            for (auto source : sources) {
                transportSourceContainer->removeTransportSource(source);
            }
            audioResources.erase(it);
            break;
        }
    }
}

void AudioResourceContainer::removeAudioResourcesForTrack (AudioTrack *track)
{
    jassert(track != nullptr);
    if (track != nullptr) {
        for (auto it = audioResources.begin(); it != audioResources.end(); it++) {
            if ((*it).first.get() == track)
                audioResources.erase(it++);
        }
    }
}

int AudioResourceContainer::getNumAudioResources() const
{
    return static_cast<int>(audioResources.size());
}

std::vector<std::shared_ptr<AudioResource>> AudioResourceContainer::resourcesAtAbsolutePosition(double positionInSeconds) const
{
    std::vector<std::shared_ptr<AudioResource>> resources;
    
    for (auto resource : audioResources)
    {
        if (resource.second->containsAbsolutePosition(positionInSeconds, audium::seconds))
        {
            resources.push_back(resource.second);
        }
    }
    
    return resources;
}

void AudioResourceContainer::cleanup()
{
    audioResources.clear();
}

std::vector<std::shared_ptr<AudioResource>> AudioResourceContainer::getAudioResourcesForTrack(AudioTrack *track) const
{
    std::vector<std::shared_ptr<AudioResource>> result;
    for (auto itr = audioResources.begin(); itr != audioResources.end(); itr++)
    {
        if (itr->first.get() == track)
        {
            result.push_back(itr->second);
        }
    }
    return result;
}

std::vector<std::shared_ptr<AudioResource>> AudioResourceContainer::getAudioResourcesForSubGroup(const AudioSubGroup *subGroup) const
{
    std::vector<std::shared_ptr<AudioResource>> result;
    for (auto itr = audioResources.begin(); itr != audioResources.end(); itr++)
    {
        if (itr->second->getAudioSubGroup().get() == subGroup)
        {
            jassert(itr->first.get() == &subGroup->getAudioTrack());
            result.push_back(itr->second);
        }
    }
    return result;
}

std::vector<std::shared_ptr<AudioResource>> AudioResourceContainer::getAudioResourcesForTrackAtAbsoluteRange(AudioTrack *track, juce::Range<double> rangeInSeconds) const
{
    
    std::vector<std::shared_ptr<AudioResource>> result;
    for (auto itr = audioResources.begin(); itr != audioResources.end(); itr++)
    {
        /// TODO: also check on end
        if (itr->first.get() == track &&
            itr->second->containsAbsolutePosition(rangeInSeconds.getStart(), audium::seconds))
        {
            result.push_back(itr->second);
        }
    }
    return result;
}

std::vector<std::shared_ptr<AudioResource>> AudioResourceContainer::getAudioResourcesForChannel(const AudioChannel *channel) const
{
    std::vector<std::shared_ptr<AudioResource>> result;
    for (const auto &itr : audioResources) {
        
        if (itr.first.get() == &channel->getAudioTrack()) {
            auto resource = itr.second;
            auto destChannel = resource->getChannelMapping().getDestinationChannel();
            if (destChannel == channel->getChannelNumber()) {
                result.push_back(resource);
            }
        }
        
    }
    
    return result;

    
}


std::shared_ptr<AudioTrack> AudioResourceContainer::getAudioTrackForResource(std::shared_ptr<AudioResource> resource) const
{
    for (auto itr = audioResources.begin(); itr != audioResources.end(); itr++)
    {
        if (itr->second == resource)
        {
            return itr->first;
        }
    }
    return nullptr;
    
}

std::vector<std::shared_ptr<AudioTrack>> AudioResourceContainer::getAudioTracks() const
{
    std::vector<std::shared_ptr<AudioTrack>> result;
    for(auto it = audioResources.begin(), end = audioResources.end(); it != end; it++)
    {
        if (std::find(result.begin(), result.end(), it->first) == result.end())
        {
            result.push_back(it->first);
        }
    }
    return result;
}

void AudioResourceContainer::onDeleteChannel(AudioTrack* audioTrack, AudioChannel* channel)
{
    std::vector<std::shared_ptr<AudioResource>> resourcesToRemove;
    
    for (auto it = audioResources.begin(), end = audioResources.end(); it != end; it++)
    {
        if (it->first.get() == audioTrack)
        {
            if (it->second->getChannelMapping().deleteChannel(channel->getChannelNumber()))
            {
                // no more channel mapping -> remove
                resourcesToRemove.push_back(it->second);
            }
        }
    }
    
    for (auto resource : resourcesToRemove)
    {
        removeAudioResource(resource);
    }
}


