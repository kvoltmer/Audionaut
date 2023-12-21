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
    
    return true;
}

int AudioGroup::getNumChannels() const
{
    int numChannels = 0;
    
    auto resources = getAudioResources();
    for (auto resource : resources)
    {
        numChannels = std::max(numChannels, static_cast<int>(resource->getChannelPosition() + resource->getNumChannels()));
    }
    
    return numChannels;
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
