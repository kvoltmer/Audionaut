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

AudioResourceContainer::~AudioResourceContainer()
{
    audioResources.clear();
}

std::shared_ptr<AudioResource> AudioResourceContainer::addAudioResource (juce::URL url,
                                                                         std::shared_ptr<AudioTrack> track,
                                                                         std::shared_ptr<AudioSubGroup> subGroup,
                                                                         int channelPosition)
{
    jassert(track != nullptr);
    jassert(subGroup != nullptr);

    // TODO: look for existing audio resource
    auto audioResource = AudioResourceFactory::createAudioResource(url,
                                                                   *this,
                                                                   track,
                                                                   subGroup,
                                                                   channelPosition);
    jassert(audioResource);
    audioResources.push_back({track, audioResource});

    return audioResource;
}

std::shared_ptr<AudiumTransportSource> AudioResourceContainer::createTransportSourceForAudioResource(std::shared_ptr<AudioResource> audioResource)
{
    auto audioFormatReaderSource = AudioResourceFactory::createAudioFormatReaderSource(audioResource->getUrl(), *formatManager.get());
                                                            
    if (audioFormatReaderSource != nullptr)
    {
        return audioResource->createNewTransportSource(audioFormatReaderSource);
    }
    return nullptr;
}

void AudioResourceContainer::removeAudioResource(std::shared_ptr<AudioResource> resource)
{
    for (auto it = audioResources.begin(); it != audioResources.end();) {
        if ((*it).second == resource) {
            audioResources.erase(it);
            break;
        }
        ++it;
    }
}

void AudioResourceContainer::removeAudioResourcesForTrack (AudioTrack *track)
{
    jassert(track != nullptr);
    if (track != nullptr)
    {        
        for (auto it = audioResources.begin(); it != audioResources.end();)
        {
            if ((*it).first.get() == track)
            {
                audioResources.erase(it++);
            }
            else
            {
                ++it;
            }
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
            if (it->second->deleteChannel(channel))
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

void AudioResourceContainer::deselectAllResources()
{
    for (auto it = audioResources.begin(), end = audioResources.end(); it != end; it++)
    {
        it->second->setSelected(false, false);
    }
    
    sendActionMessage(audioResourceSelectedAction);
}


