/*
  ==============================================================================

    AudioGroup.cpp
    Created: 28 Sep 2023 1:33:20pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "Engine/Group/AudioGroup.h"
#include "Engine/Group/AudioClip.h"
#include "Engine/AudioResourceContainer.h"
#include "Engine/AudioRegionContainer.h"
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
    for (auto subGroup : audioSubGroups)
    {
        subGroup->cleanup();
    }
    audioSubGroups.clear();
    
    audioChannels.clear();
    playListContainer->cleanup();
    transportSourceContainer->cleanup();
}


std::vector<std::shared_ptr<AudioResource>> AudioGroup::getAudioResources() const
{
    return audioResourceContainer.getAudioResourcesForGroup(const_cast<AudioGroup*>(this));
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
    // Group
    outputStream.writeInt(groupId);
    outputStream.writeString(getName());
    outputStream.writeString(currentColour.toString());
    
    // Channels
    outputStream.writeInt(static_cast<int>(audioChannels.size()));
    for (auto channel : audioChannels)
    {
        channel->writeToStream(outputStream);
    }
    
    // SubGroups
    outputStream.writeInt(static_cast<int>(audioSubGroups.size()));
    for (auto subGroup : audioSubGroups)
    {
        subGroup->writeToStream(outputStream);
    }
    
    // PlayList
    getPlayListContainer()->writeToStream(outputStream);
    
    return true;
}

bool AudioGroup::readFromStream (juce::InputStream& inputStream)
{
    cleanup();
    
    // Group
    groupId         = inputStream.readInt();
    groupName       = inputStream.readString();
    currentColour   = juce::Colour::fromString(inputStream.readString());
    
    // Channels
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

    jassert(audioSubGroups.size() == 0);
    jassert(nextSubGroupId == 0);
    
    // SubGroups
    auto numSubGroups = inputStream.readInt();
        
    for (auto g = 0; g < numSubGroups; g++)
    {
        auto subGroup = AudioGroupFactory::createAudioSubGroup(*this);
        audioSubGroups.push_back(subGroup);
        if (!subGroup->readFromStream(inputStream))
            return false;
        nextSubGroupId = juce::jmax(nextSubGroupId, subGroup->getId());
    }
    
    // PlayList
    getPlayListContainer()->readFromStream(inputStream);
    
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
        auto channel = std::shared_ptr<AudioChannel>(new AudioChannel(*this));
        audioChannels.push_back(channel);
    }
}

int AudioGroup::getChannelNumberFor(AudioChannel* audioChannel)
{
    int number = 0;
    for (auto channel : audioChannels)
    {
        if (channel.get() == audioChannel)
        {
            return number;
        }
        number++;
    }
    
    jassertfalse;
    return 0;
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
                level += resource->getAudioTransportSource()->getOutputLevel(channelNumber);
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
    return 0.0;
}


std::shared_ptr<AudioSubGroup> AudioGroup::createNewAudioSubGroup(double transportPosition, int subGroupId)
{
    subGroupId = (subGroupId < 0) ? getNextSubGroupId() : subGroupId;
    jassert( !subGroupIdExists(subGroupId) );
    
    auto subGroup = AudioGroupFactory::createAudioSubGroup(*this);
    subGroup->setId(subGroupId);
    subGroup->getAudioClip()->setAbsolutePosition(transportPosition, audium::seconds);
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

void AudioGroup::setChannelHeight(int height)
{
    for (auto i = 0; i < getNumChannels(); i++)
    {
        getChannel(i)->setChannelHeight(height);
    }
}

std::vector<std::shared_ptr<PositionableBase>> AudioGroup::getPositionableItems(bool arrangementMode) const
{
    // returns all items in the timeline
    // note: depending on the arrangement or edit mode
    
    std::vector<std::shared_ptr<PositionableBase>> result;
    if (arrangementMode)
    {
        auto playListItems = getPlayListContainer()->getPlayListItems();
        for (auto playListItem : playListItems)
            result.push_back(playListItem);
    }
    else
    {
        for (auto subGroup : getAudioSubGroups())
            result.push_back(subGroup->getAudioClip());
    }
    return result;
}

void AudioGroup::deleteSelectedSubGroups()
{
    for (int i = static_cast<int>(audioSubGroups.size())-1; i >= 0; i--)
    {
        if (audioSubGroups[i]->isSelected())
        {
            deleteSubGroup(i);
        }
    }
    getAudioResourceContainer().sendActionMessage(rebuildAll);
}

void AudioGroup::deleteSubGroup(int atIndex)
{
    if (atIndex >= 0 && atIndex < audioSubGroups.size())
    {
        audioRegionContainer.deleteAudioRegionsForSubGroup(audioSubGroups[atIndex]);
        audioSubGroups.erase(audioSubGroups.begin() + atIndex);
    }
}

void AudioGroup::deselectAllSubGroups()
{
    for (auto subGroup : audioSubGroups)
        subGroup->setSelected(false);
}

void AudioGroup::deselectAllChannels()
{
    for (auto channel : audioChannels)
        channel->setSelected(false);
}

juce::SparseSet<int> AudioGroup::getSelectedRows() const
{
    juce::SparseSet<int> result;
    for (auto i = 0; i < getNumChannels(); i++)
    {
        if (getChannel(i) != nullptr &&
            getChannel(i)->isSelected())
        {
            result.addRange ({i, i + 1});
        }
    }
    return result;
}

void AudioGroup::setSelectedRows(juce::SparseSet<int>& selectedRows)
{
    deselectAllChannels();
    for (auto i = 0; i < selectedRows.size(); i++)
    {
        if (auto channel = getChannel(selectedRows[i]))
        {
            channel->setSelected(true);
        }
    }
}

void AudioGroup::deleteSelectedChannels()
{
    auto selected = getSelectedRows();
    for (int i = selected.size()-1; i >= 0; i--)
    {
        auto channel = getChannel(selected[i]);
        
        deleteChannel(channel);
    }
    getAudioResourceContainer().sendActionMessage(rebuildAll);
}

void AudioGroup::deleteChannel(std::shared_ptr<AudioChannel> channel)
{
    auto it = std::find(audioChannels.begin(), audioChannels.end(), channel);
    if (it != audioChannels.end())
    {
        audioResourceContainer.onDeleteChannel(channel);
        audioChannels.erase(it);
    }
    
    // cleanup subgroups
    std::vector<std::shared_ptr<AudioSubGroup>> subGroupsToDelete;
    for (auto subGroup : audioSubGroups)
    {
        if (subGroup->getAudioResources().size() == 0)
        {
            subGroupsToDelete.push_back(subGroup);
        }
    }
    
    for (auto item : subGroupsToDelete)
    {
        auto it = std::find(audioSubGroups.begin(), audioSubGroups.end(), item);
        if (it != audioSubGroups.end())
        {
            deleteSubGroup(static_cast<int>(std::distance(audioSubGroups.begin(), it)));
        }
    }
}

    



