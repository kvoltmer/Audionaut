/*
  ==============================================================================

    AudioGroup.cpp
    Created: 28 Sep 2023 1:33:20pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "Engine/Group/AudioGroup.h"
#include "Engine/Group/AudioGroupContainer.h"
#include "Engine/Group/AudioClip.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/Resource/AudioResource.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/TransportSourceContainer.h"
#include "Engine/Factory/AudioGroupFactory.h"
#include "Engine/Channel/AudioChannel.h"
#include "Engine/AudiumTransportSource.h"
#include "Engine/Undo/UndoableContainerAction.h"

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
    
    nextSubGroupId = 0;
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
    groupColour = colour;
}

bool AudioGroup::writeToJson (json& output)
{
    output["id"] = groupId;
    output["name"] = groupName;
    output["colour"] = groupColour.toString().toStdString();
    
    for (auto channel : audioChannels)
    {
        output["channels"] += channel->data;
    }
    
    for (auto subGroup : audioSubGroups)
    {
        json j;
        subGroup->writeToJson(j);
        output["subGroups"] += j;
    }
    
    json playList;
    playListContainer->writeToJson(playList);
    output["playList"] = playList;
    
    return true;
}

bool AudioGroup::writeToStream (juce::OutputStream& outputStream)
{
    return audium::Streamable::writeToStream(outputStream);
}

bool AudioGroup::readFromJson (json& input)
{
    cleanup();
    
    groupId = input["id"].template get<int>();;
    groupName = input["name"].template get<std::string>();
    groupColour = juce::Colour::fromString(input["colour"].template get<std::string>());
    
    // Channels
    auto jsonChannels = input["channels"];
    for (auto& jsonElement : jsonChannels)
    {
        auto channel = addChannel();
        channel->data = jsonElement;
    }
    
    // SubGroups
    auto jsonSubGroups = input["subGroups"];
    for (auto& jsonElement : jsonSubGroups)
    {
        auto subGroup = AudioGroupFactory::createAudioSubGroup(*this);
        audioSubGroups.push_back(subGroup);
        
        if (!subGroup->readFromJson(jsonElement))
            return false;
        nextSubGroupId = juce::jmax(nextSubGroupId, subGroup->getId());
        
    }
    
    // PlayList
    auto jsonPlayList = input["playList"];
    playListContainer->readFromJson(jsonPlayList);
    
    return true;
}

bool AudioGroup::readFromStream (juce::InputStream& inputStream)
{
    if (audium::Streamable::readFromStream(inputStream))
    {
        getAudioGroupContainer().sendActionMessage(updateAll);
        return true;
    }
    return false;
}

int AudioGroup::getSizeInUnits()
{
    return (int)audioSubGroups.size() * 16;
}

int AudioGroup::getNumChannels() const
{
    return static_cast<int>(audioChannels.size());
}

void AudioGroup::ensureNumChannels(int channelsNeeded)
{
    while (getNumChannels() < channelsNeeded)
    {
        addChannel();
    }
}

std::shared_ptr<AudioChannel> AudioGroup::addChannel()
{
    auto channel = std::shared_ptr<AudioChannel>(new AudioChannel(*this));
    audioChannels.push_back(channel);
    return channel;
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
    
    //jassertfalse;
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
    // we need a shared_ptr for this
    auto group = getAudioGroupContainer().getAudioGroupById(getId());
    jassert(group);
    
    // Undo: store old state
    auto action = std::make_unique<audium::UndoableContainerAction>(group);

    for (int i = static_cast<int>(audioSubGroups.size())-1; i >= 0; i--)
    {
        if (audioSubGroups[i]->isSelected())
        {
            deleteSubGroup(i);
        }
    }
    
    // Undo: store new state and perform
    action->storeNewState();
    getAudioGroupContainer().getUndoManager()->perform(action.release(), "Delete Selected Group(s)");
    getAudioGroupContainer().getUndoManager()->beginNewTransaction();
    
}

void AudioGroup::deleteSubGroup(int atIndex)
{
    if (atIndex >= 0 && atIndex < audioSubGroups.size())
    {
        audioSubGroups[atIndex]->cleanup();
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
    // we need a shared_ptr
    auto group = getAudioGroupContainer().getAudioGroupById(getId());
    jassert(group);
    
    // Undo: store old state
    auto action = std::make_unique<audium::UndoableContainerAction>(group);

    auto selected = getSelectedRows();
    for (int i = selected.size()-1; i >= 0; i--)
    {
        auto channel = getChannel(selected[i]);
        
        deleteChannel(channel);
    }
    
    // Undo: store new state and perform
    action->storeNewState();
    getAudioGroupContainer().getUndoManager()->perform(action.release(), "Delete Selected Channel(s)");
    getAudioGroupContainer().getUndoManager()->beginNewTransaction();
    
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

    



