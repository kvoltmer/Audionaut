/*
  ==============================================================================

    AudioGroup.cpp
    Created: 28 Sep 2023 1:33:20pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "Engine/Group/AudioGroup.h"
#include "Engine/AudioResourceContainer.h"
#include "Engine/AudioResource.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/TransportSourceContainer.h"
#include "Engine/Factory/AudioGroupFactory.h"
#include "Engine/Channel/AudioChannel.h"
#include "Engine/AudiumTransportSource.h"

AudioGroup::~AudioGroup()
{
    cleanup();
}

void AudioGroup::cleanup()
{
    playListContainer->cleanup();
    transportSourceContainer->cleanup();
}


std::vector<std::shared_ptr<AudioResource>> AudioGroup::getAudioResources() const
{
    return audioResourceContainer.getAudioResourcesForGroup(const_cast<AudioGroup*>(this));
}

std::vector<std::shared_ptr<AudioResource>> AudioGroup::getAudioResourcesAtChannelPosition(int channelPosition) const
{
    return audioResourceContainer.getAudioResourcesForGroupAtChannelPosition(const_cast<AudioGroup*>(this), channelPosition);
}

std::vector<std::shared_ptr<AudioResource>> AudioGroup::getAudioResourcesAtAbsoluteRange(juce::Range<double> rangeInSeconds) const
{
    return audioResourceContainer.getAudioResourcesForGroupAtAbsoluteRange(const_cast<AudioGroup*>(this), rangeInSeconds);
}

void AudioGroup::setColour(juce::Colour colour)
{
    currentColour = colour;
}


bool AudioGroup::writeToStream (juce::OutputStream& outputStream)
{
    outputStream.writeInt(groupId);
    outputStream.writeString(getName());
    outputStream.writeString(currentColour.toString());
    

    outputStream.writeInt(static_cast<int>(audioSubGroups.size()));
    for (auto subGroup : audioSubGroups)
    {
        subGroup->writeToStream(outputStream);
    }
    
    outputStream.writeInt(static_cast<int>(audioChannels.size()));
    for (auto channel : audioChannels)
    {
        channel->writeToStream(outputStream);
    }
    
    return true;
}

bool AudioGroup::readFromStream (juce::InputStream& inputStream)
{
    groupId         = inputStream.readInt();
    groupName       = inputStream.readString();
    currentColour   = juce::Colour::fromString(inputStream.readString());

    jassert(audioSubGroups.size() == 0);
    jassert(nextSubGroupId == 0);
    
    auto numSubGroups = inputStream.readInt();
        
    for (auto g = 0; g < numSubGroups; g++)
    {
        auto subGroup = AudioGroupFactory::createAudioSubGroup(audioResourceContainer, audioRegionContainer, *this);
        subGroup->readFromStream(inputStream);
        audioSubGroups.push_back(subGroup);
        nextSubGroupId = juce::jmax(nextSubGroupId, subGroup->getId());
    }
    
    
    auto numChannels = inputStream.readInt();
    ensureNumChannels(numChannels);
    for (auto c = 0; c < numChannels; c++)
    {
        auto channel = getChannel(c);
        if (channel != nullptr)
        {
            channel->readFromStream(inputStream);
        }
    }
    
    return true;
}

int AudioGroup::getNumChannels() const
{
    return static_cast<int>(audioChannels.size());
}

void AudioGroup::ensureNumChannels(int channelsNeeded)
{
    while (getNumChannels() < channelsNeeded)
    {
        auto channel = std::shared_ptr<AudioChannel>(new AudioChannel());
        audioChannels.push_back(channel);
    }
}

int AudioGroup::getTotalHeight() const
{
    int height = 0;
    auto channels = getNumChannels();
    for (auto c = 0; c < channels; c++)
    {
        height += getChannel(c)->getChannelHeight();
    }
    return height;
}

float AudioGroup::getOutputLevel(int channelNumber) const
{
    auto level = 0.f;
    // TODO: move this to channel class
    auto channel = getChannel(channelNumber);
    if (channel != nullptr)
    {
        auto resources = getAudioResourcesAtChannel(channelNumber);
        for (auto resource : resources)
        {
            if (resource->getAudioTransportSource()->isPlaying())
            {
                level += resource->getAudioTransportSource()->getOutputLevel();
            }
        }
    }
    
    return level;
}

std::vector<std::shared_ptr<AudioResource>> AudioGroup::getAudioResourcesAtChannel(int channelNumber) const
{
    // TODO: channel should hold a std:vector with audio resource
    //auto channel = audioGroup->getChannel(rowNumber);
    
    std::vector<std::shared_ptr<AudioResource>> result;
    auto resources = getAudioResources();
    for (auto resource : resources)
    {
        if (resource->containsChannelNumber(channelNumber))
        {
            result.push_back(resource);
        }
    }
    return result;
}

void AudioGroup::setGain(float gain, int channelNumber)
{
    // TODO: move this to channel class
    auto resources = getAudioResourcesAtChannel(channelNumber);
    for (auto resource : resources)
    {
        resource->getAudioTransportSource()->setGain(gain);
    }
}

float AudioGroup::getGain(int channelNumber) const
{
    // TODO: move this to channel class
    auto resources = getAudioResourcesAtChannel(channelNumber);
    if (resources.size() > 0)
    {
        return resources[0]->getAudioTransportSource()->getGain();
    }
    jassertfalse;
    return 0.0;
}


std::shared_ptr<AudioSubGroup> AudioGroup::createNewAudioSubGroup(const AudioResourceContainer &resourceContainer,
                                                                  const AudioRegionContainer &regionContainer,
                                                                  int subGroupId)
{
    subGroupId = (subGroupId < 0) ? getNextSubGroupId() : subGroupId;
    jassert( !subGroupIdExists(subGroupId) );
    
    auto subGroup = AudioGroupFactory::createAudioSubGroup(resourceContainer, regionContainer, *this);
    subGroup->setId(subGroupId);
    audioSubGroups.push_back(subGroup);
    std::cout << "sub group created with id = " << subGroupId << std::endl;
    return subGroup;
}

bool AudioGroup::subGroupIdExists(const int groupId) const
{
    for (auto subGroup : audioSubGroups)
    {
        if (subGroup->getId() == groupId)
            return true;
    }
    return false;
}

std::shared_ptr<AudioSubGroup> AudioGroup::getAudioSubGroupById(int groupId) const
{
    for (auto subGroup : audioSubGroups)
    {
        if (subGroup->getId() == groupId)
            return subGroup;
    }
    return nullptr;
}

std::shared_ptr<AudioSubGroup> AudioGroup::getDefaultSubGroup() const
{
    if (audioSubGroups.size() > 0)
    {
        return audioSubGroups[0];
    }
    jassertfalse;
    return  nullptr;
}
