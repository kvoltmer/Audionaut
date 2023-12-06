/*
  ==============================================================================

 AudioResourceContainer.cpp
    Created: 29 Jan 2023 12:37:15pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "Engine/AudioResourceContainer.h"
#include "Engine/AudioGroupContainer.h"
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
                                                                         int channelPosition,
                                                                         double transportPosition)
{
    std::cout << "AudioResourceContainer::addAudioResource channelPosition = " << channelPosition << " transportPosition = " << transportPosition << std::endl;
    jassert(group != nullptr);
    if (group != nullptr)
    {
        auto audioResource = AudioResourceFactory::createAudioResource(url,
                                                                       *engine.getAudioResourceContainer(),
                                                                       group,
                                                                       *formatManager.get(),
                                                                       &thread,
                                                                       channelPosition,
                                                                       transportPosition);
        
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


void AudioResourceContainer::removeAudioResource(std::shared_ptr<AudiumEngine> engine, std::shared_ptr<AudioResource> resource)
{
    for (auto it = audioResources.begin(); it != audioResources.end();)
    {
        if ((*it).second == resource)
        {
            auto region = (*it).second;
            
            auto group = (*it).first;
            group->getTransportSourceContainer()->removeTransportSource(region->getAudioTransportSource());
            
            // any resource left in group?
            if (getAudioResourcesForGroup(group.get()).size() == 0)
            {
                audioGroupContainer->removeAudioGroup(engine, group);
            }
            
            audioResources.erase(it);
            break;
        }
        ++it;
    }
    
    sendActionMessage(audioResourceRemovedAction);
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
        auto startTime = resource.second->getAbsolueStartTime();
        auto endTime = startTime + resource.second->getDurationTimeInSeconds();
        juce::Range<double> absoluteRange(startTime, endTime);
        if (absoluteRange.contains(positionInSeconds))
        {
            resources.push_back(resource.second);
        }
    }
    
    return resources;
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
        
        // url of the resource
        outputStream.writeString(resource.second->getUrlAsString());
        outputStream.writeFloat(resource.second->getAudioTransportSource()->getGain());
    }
    return true;
}

bool AudioResourceContainer::readFromStream (juce::InputStream& inputStream, const AudiumEngine& engine)
{
    jassert(audioResources.empty());
    
    auto numResources = inputStream.readInt();
    for (auto i = 0; i < numResources; i++)
    {
        // group id and name
        auto groupId = inputStream.readInt();
        auto groupName = inputStream.readString();
        auto group = audioGroupContainer->getAudioGroupById(groupId);
        if (group != nullptr && group->getName() == groupName)
        {
            jassert(group && group->getName() == groupName);
            
            // url of the resource
            auto inString = inputStream.readString();
            auto resource = addAudioResource(juce::URL(inString), engine, group);
            auto gain = inputStream.readFloat();
            resource->getAudioTransportSource()->setGain(gain);
        }
        else
        {
            return false;
        }
    }
    return true;
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

std::vector<std::shared_ptr<AudioResource>> AudioResourceContainer::getAudioResourcesForGroupAtChannelPosition(AudioGroup *group, int channelPosition) const
{
    std::vector<std::shared_ptr<AudioResource>> result;
    for (auto itr = audioResources.begin(); itr != audioResources.end(); itr++)
    {
        if (itr->first.get() == group &&
            itr->second->getChannelPosition() == channelPosition)
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


int AudioResourceContainer::getNumChannels() const
{
    // TODO: create channel class with container
    auto count = 0;

    for (auto i = 0; i < audioGroupContainer->getNumItems(); i++)
    {
        count += audioGroupContainer->getAudioGroup(i)->getNumChannels();
        
    }
    return count;
}

std::shared_ptr<AudioResource> AudioResourceContainer::getChannel(int index) const
{
    // TODO: create channel class with container
    auto channel = 0;
    
    for (auto i = 0; i < audioGroupContainer->getNumItems(); i++)
    {
        
        auto group = audioGroupContainer->getAudioGroup(i);
        
        for (auto c = 0; c < group->getNumChannels(); c++)
        {
            auto audioResources = group->getAudioResourcesAtChannelPosition(c);
            for (auto resource : audioResources)
            {
                for (auto r = 0; r < resource->getNumChannels(); r++)
                {
                    if ((channel + c + r) == index)
                    {
                        return resource;
                    }
                }
            }
        }
            
        channel += group->getNumChannels();
    }
    jassertfalse;
    return nullptr;
}
