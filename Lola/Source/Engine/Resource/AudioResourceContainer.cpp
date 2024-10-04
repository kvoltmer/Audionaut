/*
  ==============================================================================

 AudioResourceContainer.cpp
    Created: 29 Jan 2023 12:37:15pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Group/AudioGroupContainer.h"
#include "Engine/TransportSourceContainer.h"
#include "Engine/AudiumTransportSource.h"
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
                                                                         std::shared_ptr<AudioGroup> group,
                                                                         std::shared_ptr<AudioSubGroup> subGroup,
                                                                         int channelPosition)
{
    jassert(group != nullptr);
    jassert(subGroup != nullptr);

    // TODO: look for existing audio resource
    auto audioResource = AudioResourceFactory::createAudioResource(url,
                                                                   *this,
                                                                   group,
                                                                   subGroup,
                                                                   channelPosition);
    jassert(audioResource);
    audioResources.push_back({group, audioResource});

    return audioResource;
}

std::shared_ptr<AudiumTransportSource> AudioResourceContainer::createTransportSourceForAudioResource(std::shared_ptr<AudioResource> audioResource)
{
    auto audioFormatReaderSource = AudioResourceFactory::createAudioFormatReaderSource(audioResource->getUrl(), *formatManager.get());
                                                                   
    auto transportSource = audioResource->createNewTransportSource(&thread, audioFormatReaderSource);
    
    
    // obsolete?
    double sampleRate = 44100.0;
    int numSamples = 512;
    if (audioDeviceManager->getCurrentAudioDevice() != nullptr)
    {
        sampleRate = audioDeviceManager->getCurrentAudioDevice()->getCurrentSampleRate();
        numSamples = audioDeviceManager->getCurrentAudioDevice()->getCurrentBufferSizeSamples();
    }
    
    transportSource->prepareToPlay(numSamples, sampleRate);
    
    
    return transportSource;
}

void AudioResourceContainer::removeAudioResource(std::shared_ptr<AudioResource> resource)
{
    for (auto it = audioResources.begin(); it != audioResources.end();)
    {
        if ((*it).second == resource)
        {
            auto group = (*it).first;
            auto resource = (*it).second;
            audioResources.erase(it);
            break;
        }
        ++it;
    }
    
    //sendActionMessage(audioResourceRemovedAction);
}

void AudioResourceContainer::removeAudioResourcesForGroup (AudioGroup *group)
{
    jassert(group != nullptr);
    if (group != nullptr)
    {        
        for (auto it = audioResources.begin(); it != audioResources.end();)
        {
            if ((*it).first.get() == group)
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

std::vector<std::shared_ptr<AudioResource>> AudioResourceContainer::getAudioResourcesForGroup(AudioGroup *group) const
{
    std::vector<std::shared_ptr<AudioResource>> result;
    for (auto itr = audioResources.begin(); itr != audioResources.end(); itr++)
    {
        if (itr->first.get() == group)
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
            jassert(itr->first.get() == &subGroup->getAudioGroup());
            result.push_back(itr->second);
        }
    }
    return result;
}

std::vector<std::shared_ptr<AudioResource>> AudioResourceContainer::getAudioResourcesForGroupAtAbsoluteRange(AudioGroup *group, juce::Range<double> rangeInSeconds) const
{
    
    std::vector<std::shared_ptr<AudioResource>> result;
    for (auto itr = audioResources.begin(); itr != audioResources.end(); itr++)
    {
        /// TODO: also check on end
        if (itr->first.get() == group &&
            itr->second->containsAbsolutePosition(rangeInSeconds.getStart(), audium::seconds))
        {
            result.push_back(itr->second);
        }
    }
    return result;
}

std::shared_ptr<AudioGroup> AudioResourceContainer::getAudioGroupForResource(std::shared_ptr<AudioResource> resource) const
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

std::vector<std::shared_ptr<AudioGroup>> AudioResourceContainer::getAudioGroups() const
{
    std::vector<std::shared_ptr<AudioGroup>> result;
    for(auto it = audioResources.begin(), end = audioResources.end(); it != end; it++)
    {
        if (std::find(result.begin(), result.end(), it->first) == result.end())
        {
            result.push_back(it->first);
        }
    }
    return result;
}

void AudioResourceContainer::onDeleteChannel(AudioChannel* channel)
{
    std::vector<std::shared_ptr<AudioResource>> resourcesToRemove;
    
    for (auto it = audioResources.begin(), end = audioResources.end(); it != end; it++)
    {
        if (it->second->deleteChannel(channel))
        {
            // no more channel mapping -> remove
            resourcesToRemove.push_back(it->second);
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


