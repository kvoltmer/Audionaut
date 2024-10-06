/*
  ==============================================================================

    AudioGroupContainer.cpp
    Created: 10 Oct 2023 12:12:23pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudioGroupContainer.h"
#include "Engine/Group/AudioGroup.h"
#include "Engine/Group/AudioClip.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
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

void AudioGroupContainer::cleanup()
{
    transportSourceContainer->cleanup();
    audioResourceContainer->cleanup();
    
    for (auto group : audioGroups)
    {
        group->cleanup();
    }
    audioGroups.clear();
}

std::shared_ptr<AudioGroup> AudioGroupContainer::getAudioGroup(int index) const
{
    if (index >= 0 && index < audioGroups.size())
    {
        return audioGroups[index];
    }
    return nullptr;
}

std::shared_ptr<AudioGroup> AudioGroupContainer::createNewAudioGroup(const juce::String nameString)
{
    auto audioGroup = AudioGroupFactory::createAudioGroup(*this, audioResourceContainer);
    if (nameString.isEmpty())
    {
        audioGroup->setName(juce::String("Group ") + juce::String(audioGroups.size() + 1));
    }
    else
    {
        audioGroup->setName(nameString);
    }
    audioGroups.push_back(audioGroup);
    sendActionMessage(audioGroupCreatedAction);
    return audioGroup;
}

bool AudioGroupContainer::deleteAudioGroup(AudioGroup* group)
{
    auto it = std::find_if(audioGroups.begin(), audioGroups.end(), [group](const auto& item) {
        return item.get() == group;
    });
    
    if (it != audioGroups.end()) {
        group->getAudioRegionContainer()->cleanup();
        group->getAudioResourceContainer().removeAudioResourcesForGroup(group);
        audioGroups.erase(it);
        return true;
    }

    return false;
}

bool AudioGroupContainer::deleteAudioGroup(std::shared_ptr<AudioGroup> group)
{
    return deleteAudioGroup(group.get());
}

void AudioGroupContainer::deleteSelectedObjects()
{
    // Undo: store old state
    auto action = std::make_unique<audium::UndoableContainerAction>(*this);

    auto objects = selectionManager->getSelectedObjects();
    
    for (auto object : objects) {
        if (auto group = dynamic_cast<AudioGroup*>(object.get())) {
            deleteAudioGroup(group);
        }
        else {
            for (auto g : audioGroups) {
                if (g->deleteSelectedObject(object))
                    continue;
            }
        }
    }
    
    selectionManager->clear();
    
    // Undo: store new state and perform
    action->storeNewState();
    undoManager->perform(action.release(), "Delete Selected Objects(s)");
    undoManager->beginNewTransaction();
    
}

bool AudioGroupContainer::writeToStream (juce::OutputStream& outputStream)
{
    return audium::Streamable::writeToStream(outputStream);
}

bool AudioGroupContainer::readFromStream (juce::InputStream& inputStream, bool rebuild)
{
    if (audium::Streamable::readFromStream(inputStream, rebuild))
    {
        sendActionMessage(rebuild ? rebuildAll : updateAll);
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

bool AudioGroupContainer::readFromJson (json& input, bool rebuild)
{
    //std::cout << "AudioGroupContainer::readFromJson " << rebuild << std::endl;
    
    if (rebuild)
    {
        cleanup();
        jassert(audioGroups.size() == 0);
        jassert(audioResourceContainer != nullptr);
    }
    
    auto jsonGroups = input["groups"];
    int count = 0;
    for (auto& jsonElement : jsonGroups)
    {
        std::shared_ptr<AudioGroup> audioGroup = nullptr;
        if (rebuild)
        {
            audioGroup = AudioGroupFactory::createAudioGroup(*this, audioResourceContainer);
            audioGroups.push_back(audioGroup);
        }
        else
        {
            audioGroup = audioGroups[count];
        }
        
        if ( !audioGroup->readFromJson(jsonElement, rebuild))
            return false;
        
        count++;
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

void AudioGroupContainer::selectAllGroups(bool bSelected, bool selectChildren)
{
    for (auto group : audioGroups)
        group->setSelected(bSelected, selectChildren);
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
    getSelectionManager()->deselectAll();
    for (auto i = 0; i < selectedRows.size(); i++)
    {
        if (auto group = getAudioGroup(selectedRows[i]))
        {
            group->setSelected(true, false);
        }
    }
}


bool AudioGroupContainer::isSomethingSelected()
{
    return selectionManager->isSomethingSelected();
}
