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
}


std::vector<std::shared_ptr<AudioResource>> AudioGroup::getAudioResources() const
{
    return audioResourceContainer.getAudioResourcesForGroup(const_cast<AudioGroup*>(this));
}

std::vector<std::shared_ptr<AudioResource>> AudioGroup::getAudioResourcesAtAbsoluteRange(juce::Range<double> rangeInSeconds) const
{
    return audioResourceContainer.getAudioResourcesForGroupAtAbsoluteRange(const_cast<AudioGroup*>(this), rangeInSeconds);
}

std::shared_ptr<AudioSubGroup> AudioGroup::getSubGroupAtAbsolutePosition(double position, audium::TimeContextType context) const
{
    std::shared_ptr<AudioSubGroup> subGroup = nullptr;
    for (auto resource : getAudioResources())
    {
        if (resource->containsAbsolutePosition(position, context))
        {
            return resource->getAudioSubGroup();
        }
    }
    return nullptr;
}

void AudioGroup::setColour(juce::Colour colour)
{
    groupColour = colour;
}

bool AudioGroup::writeToJson (json& output)
{
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
        output["sub_groups"] += j;
    }
    
    
    playListContainer->writeToJson(output);
    
    //std::cout << output.dump(4) << std::endl;

    return true;
}

bool AudioGroup::writeToStream (juce::OutputStream& outputStream)
{
    return audium::Streamable::writeToStream(outputStream);
}

bool AudioGroup::readFromJson (json& input, bool rebuild)
{
    //std::cout << input.dump(4) << std::endl;
    if (rebuild)
        cleanup();
    
    groupName = input["name"].template get<std::string>();
    groupColour = juce::Colour::fromString(input["colour"].template get<std::string>());
    
    // Channels
    auto jsonChannels = input["channels"];
    auto c = 0;
    for (auto& jsonElement : jsonChannels)
    {
        std::shared_ptr<AudioChannel> channel = nullptr;
        if (rebuild)
        {
            channel = addChannel();
        }
        else
        {
            channel = audioChannels[c];
        }
        
        channel->data = jsonElement;
        c++;
    }
    
    // SubGroups
    auto jsonSubGroups = input["sub_groups"];
    auto i = 0;
    for (auto& jsonElement : jsonSubGroups)
    {
        std::shared_ptr<AudioSubGroup> subGroup = nullptr;
        if (rebuild)
        {
            subGroup = AudioGroupFactory::createAudioSubGroup(*this);
            audioSubGroups.push_back(subGroup);
        }
        else
        {
            subGroup = audioSubGroups[i];
        }
        
        if (!subGroup->readFromJson(jsonElement, rebuild))
            return false;
        
        i++;
    }
    
    // PlayList
    return playListContainer->readFromJson(input, rebuild);
}

bool AudioGroup::readFromStream (juce::InputStream& inputStream, bool rebuild)
{
    if (audium::Streamable::readFromStream(inputStream))
    {
        getAudioGroupContainer().sendActionMessage(rebuildAll);
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
    auto channel = std::shared_ptr<AudioChannel>(new AudioChannel(*this, (int)audioChannels.size()));
    audioChannels.push_back(channel);
    return channel;
}

std::shared_ptr<AudioChannel> AudioGroup::getChannel(int channelNumber) const
{
    if (channelNumber < audioChannels.size())
    {
        jassert(audioChannels[channelNumber]->getChannelNumber() == channelNumber);
        return audioChannels[channelNumber];
    }
    return nullptr;
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
    auto channel = getChannel(channelNumber);
    if (channel != nullptr)
    {
        auto resources = getAudioResourcesAtChannel(channelNumber);
        for (auto resource : resources)
        {
            if (resource->getAudioTransportSource()->isPlaying())
            {
                level += resource->getAudioTransportSource()->getOutputLevel(channelNumber - resource->getChannelPosition());
            }
        }
    }
    
    return level;
}

std::vector<std::shared_ptr<AudioResource>> AudioGroup::getAudioResourcesAtChannel(int channelNumber) const
{
    auto channel = getChannel(channelNumber);
    
    std::vector<std::shared_ptr<AudioResource>> result;
    for (auto resource : getAudioResources())
    {
        if (resource->containsChannel(channel))
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


std::shared_ptr<AudioSubGroup> AudioGroup::createNewAudioSubGroup(double transportPosition, audium::TimeContextType context)
{
    auto subGroup = AudioGroupFactory::createAudioSubGroup(*this);
    subGroup->getAudioClip()->setAbsolutePosition(transportPosition, context);
    audioSubGroups.push_back(subGroup);
    return subGroup;
}

std::shared_ptr<AudioSubGroup> AudioGroup::getSharedPtr(const AudioSubGroup* sub) const
{
    for (auto subGroup : audioSubGroups)
    {
        if (subGroup.get() == sub)
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
    
    // Undo: store old state
    auto action = std::make_unique<audium::UndoableContainerAction>(getAudioGroupContainer());

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
    // Undo: store old state
    auto action = std::make_unique<audium::UndoableContainerAction>(getAudioGroupContainer());

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
    
    auto count = 0;
    for (auto channel : audioChannels)
    {
        channel->setChannelNumber(count++);
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

    



