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

void AudioGroupContainer::init(AudioResourceContainer *resourceContainer)
{
    audioResourceContainer = resourceContainer;
}

void AudioGroupContainer::cleanup()
{
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

std::shared_ptr<AudioGroup> AudioGroupContainer::getSharedPtr(const AudioGroup* g) const
{
    for (auto group : audioGroups)
    {
        if (group.get() == g)
            return group;
    }
    return nullptr;
}

std::shared_ptr<AudioGroup> AudioGroupContainer::createNewAudioGroup(AudioResourceContainer &audioResourceContainer,
                                                                     const juce::String nameString)
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

bool AudioGroupContainer::deleteAudioGroup(std::shared_ptr<AudioGroup> group)
{
    
    group->getAudioRegionContainer()->deleteAudioRegionsForGroup(group);
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
    auto action = std::make_unique<audium::UndoableContainerAction>(getptr());
    
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
    jassert(audioResourceContainer != nullptr);
    
    auto jsonSubGroups = input["groups"];
    for (auto& jsonElement : jsonSubGroups)
    {
        auto audioGroup = AudioGroupFactory::createAudioGroup(*this, *audioResourceContainer);
        audioGroups.push_back(audioGroup);
        if ( !audioGroup->readFromJson(jsonElement))
            return false;
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

void AudioGroupContainer::createRegionsFromSelection(juce::String name)
{
    // Undo: store old state
    auto action = std::make_unique<audium::UndoableContainerAction>(getptr());
    
    for (auto i = 0; i < getNumItems(); i++)
    {
        if (auto group = getAudioGroup(i))
        {
            // TODO: mode?
            //if (playListScheduler->isArrangementMode())
//            {
//                if (auto item = group->getPlayListContainer()->itemAtAbsoluteRange(selectedPositionClocks, audium::clocks))
//                {
//                    // we need the start of the actual audio file
//                    auto localStart = selectedPositionClocks.getStart() - item->getAbsolutePosition(audium::clocks) + item->getRegionData(audium::clocks).getStart();
//                    
//                    juce::Range<double> localRange(localStart, localStart + selectedPositionClocks.getLength());
//                    auto localRangeInSeconds = playListScheduler->getTempoProvider()->clocksToSeconds(localRange);
//                    createRegion(name, localRangeInSeconds, group, item->getRegion()->getAudioSubGroup());
//                }
//            }
//            else
//            {
//                // get resources at this range
//                auto rangeInSeconds = playListScheduler->getTempoProvider()->clocksToSeconds(selectedPositionClocks);
//                // TODO: change this to subgroups
//                auto resources = group->getAudioResourcesAtAbsoluteRange(rangeInSeconds);
//                if (resources.size() > 0)
//                {
//                    // grab the first valid resource
//                    auto resource  = resources[0];
//                    
//                    auto maxLength = 0.0;
//                    for (auto res : resources)
//                        maxLength = std::max(maxLength, res->getFileLength(audium::seconds));
//                    
//                    const auto transportPosition = resource->getAudioSubGroup()->getAudioClip()->getAbsolutePosition(audium::seconds);
//                    rangeInSeconds -= transportPosition;
//                    const auto startPosition = resource->getAudioSubGroup()->getAudioClip()->getRegionData(audium::seconds).getStart();
//                    rangeInSeconds += startPosition;
//                    
//                    if (rangeInSeconds.getEnd() > maxLength)
//                        rangeInSeconds.setEnd(maxLength);
//                    
//                    createRegion(name, rangeInSeconds, group, resource->getAudioSubGroup());
//                }
//            }
        }
    }
    // clear selection
    selectedPositionClocks = juce::Range<double>();
    
    
    // Undo: store new state
    action->storeNewState();
    getUndoManager()->perform(action.release(), "Create Region(s)");
    getUndoManager()->beginNewTransaction();
}

void AudioGroupContainer::setSelectedPosition(juce::Range<double> pos, audium::TimeContextType context)
{
    if (context == audium::seconds)
    {
        selectedPositionClocks = tempoProvider->secondsToClocks(pos);
    }
    else if (context == audium::clocks)
    {
        selectedPositionClocks = pos;
    }
    else
    {
        jassertfalse;
    }
}

juce::Range<double> AudioGroupContainer::getSelectedPosition(audium::TimeContextType context) const
{
    if (context == audium::seconds)
    {
        return tempoProvider->clocksToSeconds(selectedPositionClocks);
    }
    else if (context == audium::clocks)
    {
        return selectedPositionClocks;
    }

    jassertfalse;
    return juce::Range<double>();
}

