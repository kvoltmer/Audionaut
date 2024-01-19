/*
  ==============================================================================

 AudioResourceContainer.cpp
    Created: 29 Jan 2023 12:37:15pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "Engine/AudioResourceContainer.h"
#include "Engine/Group/AudioGroupContainer.h"
#include "Engine/TransportSourceContainer.h"
#include "Engine/AudiumTransportSource.h"
#include "Engine/ActionMessages.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Factory/AudioResourceFactory.h"

AudioResourceContainer::~AudioResourceContainer()
{
    audioResources.clear();
}

std::shared_ptr<AudioResource> AudioResourceContainer::addAudioResource (juce::URL url,
                                                                         const AudiumEngine& engine,
                                                                         std::shared_ptr<AudioGroup> group,
                                                                         std::shared_ptr<AudioSubGroup> subGroup,
                                                                         int channelPosition,
                                                                         double transportPosition,
                                                                         int resourceId)
{
    std::cout << "AudioResourceContainer::addAudioResource channelPosition = " << channelPosition << " transportPosition (seconds) = " << transportPosition << std::endl;
    jassert(group != nullptr);
    if (group != nullptr)
    {
        resourceId = (resourceId < 0) ? getNextId() : resourceId;
        auto audioResource = AudioResourceFactory::createAudioResource(url,
                                                                       *engine.getAudioResourceContainer(),
                                                                       group,
                                                                       subGroup,
                                                                       *formatManager.get(),
                                                                       &thread,
                                                                       channelPosition,
                                                                       transportPosition,
                                                                       resourceId);
        
        double sampleRate = 44100.0;
        int numSamples = 512;
        if (audioDeviceManager->getCurrentAudioDevice() != nullptr)
        {
            sampleRate = audioDeviceManager->getCurrentAudioDevice()->getCurrentSampleRate();
            numSamples = audioDeviceManager->getCurrentAudioDevice()->getCurrentBufferSizeSamples();
        }
        if (audioResource &&
            audioResource->getAudioTransportSource())
        {
//            auto channelsNeeded = channelPosition + audioResource->getNumChannels();
//            group->ensureNumChannels(channelsNeeded);
            
            audioResource->getAudioTransportSource()->prepareToPlay(numSamples, sampleRate);
            audioResources.push_back({group, audioResource});
            sendActionMessage(audioResourceCreatedAction);
            return audioResource;
        }
    }
    
    return nullptr;
}

void AudioResourceContainer::prepareToPlay (double sampleRate, int blockSize)
{
    for (auto resource : audioResources)
    {
        resource.second->getAudioTransportSource()->prepareToPlay(blockSize, sampleRate);
    }
}


void AudioResourceContainer::removeAudioResource(std::shared_ptr<AudioResource> resource)
{
    for (auto it = audioResources.begin(); it != audioResources.end();)
    {
        if ((*it).second == resource)
        {
            auto group = (*it).first;
            auto resource = (*it).second;
            
            group->getTransportSourceContainer()->removeTransportSource(resource->getAudioTransportSource());
            
            audioResources.erase(it);
            
            // any resource left in group?
            if (getAudioResourcesForGroup(group.get()).size() == 0)
            {
                audioGroupContainer->deleteAudioGroup(group);
            }
            
            
            break;
        }
        ++it;
    }
    
    //sendActionMessage(audioResourceRemovedAction);
}

void AudioResourceContainer::removeAudioResourcesForGroup (std::shared_ptr<AudioGroup> group)
{
    jassert(group != nullptr);
    if (group != nullptr)
    {        
        for (auto it = audioResources.begin(); it != audioResources.end();)
        {
            if ((*it).first == group)
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

int AudioResourceContainer::getNumAudioGroups() const
{
    jassert(static_cast<int>(getAudioGroups().size()) == audioGroupContainer->getNumItems());
    return audioGroupContainer->getNumItems();
}

std::shared_ptr<AudioResource> AudioResourceContainer::getAudioResource(int index) const
{
    if (index < getNumAudioResources())
    {
        auto it = audioResources.begin();
        std::advance(it, index);
        return it->second;
    }
    return nullptr;
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

std::shared_ptr<AudioResource> AudioResourceContainer::getAudioResourceById(int resourceId) const
{
    for (auto resource : audioResources)
    {
        if (resource.second->getId() == resourceId)
        {
            return resource.second;
        }
    }
    jassertfalse;
    return nullptr;
}


std::shared_ptr<AudioGroup> AudioResourceContainer::getAudioGroup(int index) const
{
    jassert(index < getNumAudioGroups());
    return getAudioGroups()[index];
}

bool AudioResourceContainer::writeToStream (juce::OutputStream& outputStream)
{
    outputStream.writeInt(static_cast<int>(audioResources.size()));
        
    for (auto resource : audioResources)
    {
        // group id and name
        outputStream.writeInt(resource.first->getId());
        outputStream.writeString(resource.first->getName());
        
        // sub group id
        outputStream.writeInt(resource.second->getAudioSubGroup()->getId());
        
        // write audio resource data
        resource.second->writeToStream(outputStream);
    }
    return true;
}

bool AudioResourceContainer::readFromStream (juce::InputStream& inputStream, const AudiumEngine& engine)
{
    jassert(audioResources.empty());
    jassert(nextId == 0);
    
    auto numResources = inputStream.readInt();
    for (auto i = 0; i < numResources; i++)
    {
        // group id and name
        auto groupId =      inputStream.readInt();
        auto groupName =    inputStream.readString();
        
        // sub group id
        auto subGroupId =   inputStream.readInt();
        
        auto group = audioGroupContainer->getAudioGroupById(groupId);
        auto subGroup = group->getAudioSubGroupById(subGroupId);
        if (group != nullptr &&
            group->getName() == groupName &&
            subGroup != nullptr)
        {
            // url of the resource
            auto streamPos = inputStream.getPosition();
            auto url = inputStream.readString();
            auto resource = addAudioResource(juce::URL(url), engine, group, subGroup);
            inputStream.setPosition(streamPos);
            
            // audio resource
            resource->readFromStream(inputStream);
            auto channelsNeeded = resource->getChannelPosition() + resource->getNumChannels();
            group->ensureNumChannels(channelsNeeded);
        }
        else
        {
            jassertfalse;
            return false;
        }
    }
    return true;
}

void AudioResourceContainer::cleanup()
{
    audioResources.clear();
    nextId = 0;
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

std::vector<std::shared_ptr<AudioResource>> AudioResourceContainer::getAudioResourcesForGroupAndSubGroup(const AudioGroup *group, const AudioSubGroup *subGroup) const
{
    std::vector<std::shared_ptr<AudioResource>> result;
    for (auto itr = audioResources.begin(); itr != audioResources.end(); itr++)
    {
        if (itr->first.get() == group &&
            itr->second->getAudioSubGroup().get() == subGroup)
        {
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

void AudioResourceContainer::onDeleteChannel(std::shared_ptr<AudioChannel> channel)
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


