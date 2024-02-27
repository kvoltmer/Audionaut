/*
  ==============================================================================

    AudioGroupContainer.cpp
    Created: 10 Oct 2023 12:12:23pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudioGroupContainer.h"
#include "Engine/Group/AudioGroup.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/AudiumEngine.h"
#include "Engine/ActionMessages.h"
#include "Engine/TransportSourceContainer.h"
#include "Engine/Factory/AudioGroupFactory.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/Undo/UndoableContainerAction.h"

AudioGroupContainer::~AudioGroupContainer()
{
    undoManager = nullptr;
    jassert(audioGroups.empty());
}

void AudioGroupContainer::init(AudioResourceContainer *resourceContainer,
          AudioRegionContainer *regionContainer)
{
    audioResourceContainer = resourceContainer;
    audioRegionContainer = regionContainer;
}

void AudioGroupContainer::cleanup()
{
    for (auto group : audioGroups)
    {
        group->cleanup();
    }
    audioGroups.clear();
    
    nextId = 0;
}

bool AudioGroupContainer::groupIdExists(const int groupId) const
{
    for (auto group : audioGroups)
    {
        if (group->getId() == groupId)
            return true;
    }
    return false;
}

std::shared_ptr<AudioGroup> AudioGroupContainer::getAudioGroup(int index) const
{
    if (index >= 0 && index < audioGroups.size())
    {
        return audioGroups[index];
    }
    return nullptr;
}

std::shared_ptr<AudioGroup> AudioGroupContainer::getAudioGroupById(int groupId) const
{
    for (auto group : audioGroups)
    {
        if (group->getId() == groupId)
            return group;
    }
    jassertfalse;
    return nullptr;
}



std::shared_ptr<AudioGroup> AudioGroupContainer::createNewAudioGroup(AudioResourceContainer &audioResourceContainer,
                                                                     AudioRegionContainer &audioRegionContainer,
                                                                     const juce::String nameString,
                                                                     int groupId)
{
    groupId = (groupId < 0) ? getNextId() : groupId;
    jassert( !groupIdExists(groupId) );
    
    auto audioGroup = AudioGroupFactory::createAudioGroup(*this, audioResourceContainer, audioRegionContainer);
    if (nameString.isEmpty())
    {
        audioGroup->setName(juce::String("Group ") + juce::String(groupId));
    }
    else
    {
        audioGroup->setName(nameString);
    }
    audioGroup->setId(groupId);
    audioGroups.push_back(audioGroup);
    std::cout << "audio group created with id = " << groupId << std::endl;
    sendActionMessage(audioGroupCreatedAction);
    return audioGroup;
}

bool AudioGroupContainer::deleteAudioGroup(std::shared_ptr<AudioGroup> group)
{
    
    group->getAudioRegionContainer().deleteAudioRegionsForGroup(group);
    group->getAudioResourceContainer().removeAudioResourcesForGroup(group);
    
    auto it = std::find(audioGroups.begin(), audioGroups.end(), group);
    if (it != audioGroups.end())
    {
        group->cleanup();
        audioGroups.erase(it);
        sendActionMessage(rebuildAll);
        return true;
    }
    return false;
}

void AudioGroupContainer::deleteSelectedGroups()
{
    // Undo: store old state
    auto action = std::make_unique<audium::UndoableContainerAction>(audioRegionContainer->getAudioGroupContainer());
    
    for (int i = static_cast<int>(audioGroups.size())-1; i >= 0; i--)
    {
        if (audioGroups[i]->isSelected())
        {
            deleteAudioGroup(audioGroups[i]);
        }
    }
    
    // Undo: store new state
    action->storeNewState();
    undoManager->perform(action.release(), "Delete Group(s)");
    undoManager->beginNewTransaction();
    
}

bool AudioGroupContainer::writeToStream (juce::OutputStream& outputStream)
{
    return audium::Streamable::writeToStream(outputStream);
}

bool AudioGroupContainer::readFromStream (juce::InputStream& inputStream)
{
    if (audium::Streamable::readFromStream(inputStream))
    {
        sendActionMessage(rebuildAll);
        return true;
    }
    return false;
}

bool AudioGroupContainer::writeToJson (json& output)
{
    for (auto& group : audioGroups)
    {
        json j;
        group->writeToJson(j);
        output["groups"] += j;
    }
    return true;
}

bool AudioGroupContainer::readFromJson (json& input)
{
    cleanup();
    jassert(audioGroups.size() == 0);
    jassert(nextId == 0);
    jassert(audioResourceContainer != nullptr && audioRegionContainer != nullptr);
    
    auto jsonSubGroups = input["groups"];
    for (auto& jsonElement : jsonSubGroups)
    {
        auto audioGroup = AudioGroupFactory::createAudioGroup(*this, *audioResourceContainer, *audioRegionContainer);
        audioGroups.push_back(audioGroup);
        
        audioGroup->readFromJson(jsonElement);
        nextId = juce::jmax(nextId, audioGroup->getId());
    }
    return true;
}

int AudioGroupContainer::getSizeInUnits()
{
    return getNumItems() * 8;
}

std::shared_ptr<AudioGroup> AudioGroupContainer::getDefaultGroup() const
{
    // returns the first selected group
    for (auto group : audioGroups)
    {
        if (group->isSelected())
            return group;
    }
    
    // in case nothing is selected the first group is returned
    if (audioGroups.size() > 0)
    {
        return audioGroups[0];
    }
    
    jassertfalse;
    return nullptr;
}

void AudioGroupContainer::deselectAll()
{
    for (auto group : audioGroups)
    {
        group->deselectAllChannels();
        group->setSelected(false);
    }
}

juce::SparseSet<int> AudioGroupContainer::getSelectedRows() const
{
    juce::SparseSet<int> result;
    for (auto i = 0; i < getNumItems(); i++)
    {
        if (getAudioGroup(i) != nullptr &&
            getAudioGroup(i)->isSelected())
        {
            result.addRange ({i, i + 1});
        }
    }
    return result;
}

void AudioGroupContainer::setSelectedRows(juce::SparseSet<int>& selectedRows)
{
    deselectAll();
    for (auto i = 0; i < selectedRows.size(); i++)
    {
        if (auto group = getAudioGroup(selectedRows[i]))
        {
            group->setSelected(true);
        }
    }
}


