/*
  ==============================================================================

    AudioRegionContainer.cpp
    Created: 30 May 2023 10:16:35am
    Author:  Klaus Voltmer

  ==============================================================================
*/
#include <iostream>

#include "AudioRegionContainer.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Group/AudioGroupContainer.h"
#include "Engine/Group/AudioGroup.h"
#include "Engine/Group/AudioClip.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/AudiumTransportSource.h"
#include "Engine/Undo/UndoableContainerAction.h"

std::shared_ptr<AudioRegion> AudioRegionContainer::createDefaultRegion(std::shared_ptr<AudioGroup> group)
{
//    jassert(getNumRegions(group.get()) == 0);
//    auto audioResources = audioResourceContainer->getAudioResourcesForGroup(group.get());
//    auto name = (audioResources.size() > 0) ? audioResources[0]->getFileNameWithoutExtension() : "n/a";
//    auto seconds = 0.0;
//    for (auto resource : audioResources)
//    {
//        seconds = juce::jmax(seconds, resource->getAudioTransportSource()->getLengthInSeconds());
//    }
//    jassert(seconds > 0.0);
//
//    return createRegion(name, juce::Range(0.0, seconds), group);
    jassertfalse;
    return nullptr;
}

void AudioRegionContainer::createRegionsFromSelection(juce::String name)
{
    // Undo: store old state
    auto action = std::make_unique<audium::UndoableContainerAction>(audioGroupContainer);
    
    for (auto i = 0; i < audioGroupContainer->getNumItems(); i++)
    {
        if (auto group = audioGroupContainer->getAudioGroup(i))
        {
            if (playListScheduler->isArrangementMode())
            {
                if (auto item = group->getPlayListContainer()->itemAtAbsoluteRange(selectedPositionClocks, audium::clocks))
                {
                    // we need the start of the actual audio file
                    auto localStart = selectedPositionClocks.getStart() - item->getAbsolutePosition(audium::clocks) + item->getRegionData(audium::clocks).getStart();
                    
                    juce::Range<double> localRange(localStart, localStart + selectedPositionClocks.getLength());
                    auto localRangeInSeconds = playListScheduler->getTempoProvider()->clocksToSeconds(localRange);
                    createRegion(name, localRangeInSeconds, group, item->getRegion()->getAudioSubGroup());
                }
            }
            else
            {
                // get resources at this range
                auto rangeInSeconds = playListScheduler->getTempoProvider()->clocksToSeconds(selectedPositionClocks);
                // TODO: change this to subgroups
                auto resources = group->getAudioResourcesAtAbsoluteRange(rangeInSeconds);
                if (resources.size() > 0)
                {
                    // grab the first valid resource
                    auto resource  = resources[0];
                    
                    auto maxLength = 0.0;
                    for (auto res : resources)
                        maxLength = std::max(maxLength, res->getFileLength(audium::seconds));
                    
                    const auto transportPosition = resource->getAudioSubGroup()->getAudioClip()->getAbsolutePosition(audium::seconds);
                    rangeInSeconds -= transportPosition;
                    const auto startPosition = resource->getAudioSubGroup()->getAudioClip()->getRegionData(audium::seconds).getStart();
                    rangeInSeconds += startPosition;
                    
                    if (rangeInSeconds.getEnd() > maxLength)
                        rangeInSeconds.setEnd(maxLength);
                    
                    createRegion(name, rangeInSeconds, group, resource->getAudioSubGroup());
                }
            }
        }
    }
    // clear selection
    selectedPositionClocks = juce::Range<double>();
    
    
    // Undo: store new state
    action->storeNewState();
    getUndoManager()->perform(action.release(), "Create Region(s)");
    getUndoManager()->beginNewTransaction();
}

std::shared_ptr<AudioRegion> AudioRegionContainer::createRegion(std::shared_ptr<AudioGroup> group,
                                                                std::shared_ptr<AudioSubGroup> subGroup)
{
    jassert(group != nullptr);
    jassert(subGroup != nullptr);
    auto audioRegion = std::shared_ptr<AudioRegion>(new AudioRegion(group, subGroup, playListScheduler->getTempoProvider()));
    audioRegions.push_back(audioRegion);
    // std::cout << "createRegion " << audioRegion << " index " << getRegionIndex(audioRegion) << std::endl;
    return audioRegion;
}

std::shared_ptr<AudioRegion> AudioRegionContainer::createRegion(juce::String regionName,
                                                                juce::Range<double> position,
                                                                std::shared_ptr<AudioGroup> group,
                                                                std::shared_ptr<AudioSubGroup> subGroup)
{
    if (group == nullptr)
    {
        group = audioGroupContainer->getDefaultGroup();
    }
    
    if (subGroup == nullptr)
    {
        subGroup = group->getDefaultSubGroup();
    }
    
    auto audioRegion = createRegion(group, subGroup);
    audioRegion->setRegionData(position, audium::seconds);
    audioRegion->setName(regionName);
    sendActionMessage(regionCreatedAction);
    return audioRegion;
}

void AudioRegionContainer::deleteRegion(int atIndex)
{
    auto region = getRegion(atIndex);
    region->getAudioGroup()->getPlayListContainer()->deleteAssociatedItems(region);
    
    if (atIndex >= 0 && atIndex < audioRegions.size())
    {
        audioRegions.erase(audioRegions.begin() + atIndex);
        sendActionMessage(regionDeletedAction);
    }
}

void AudioRegionContainer::deleteSelectedRegions()
{
    // Undo: store old state
    auto action = std::make_unique<audium::UndoableContainerAction>(audioGroupContainer);
        
    auto selected = getSelectedRows();
    for (int i = selected.size()-1; i >= 0; i--)
    {
        auto region = getRegion(selected[i]);
        jassert(region);
        deleteRegion(selected[i]);
    }
    
    // Undo: store new state
    action->storeNewState();
    getUndoManager()->perform(action.release(), "Delete Region(s)");
    getUndoManager()->beginNewTransaction();
}

void AudioRegionContainer::deselectAll()
{
    for (auto & region : audioRegions)
    {
        region->setSelected(false);
    }
}

juce::SparseSet<int> AudioRegionContainer::getSelectedRows() const
{
    juce::SparseSet<int> result;
    for (auto i = 0; i < getNumRegions(); i++)
    {
        if (getRegion(i) != nullptr &&
            getRegion(i)->isSelected())
        {
            result.addRange ({i, i + 1});
        }
    }
    return result;
}

void AudioRegionContainer::setSelectedRows(juce::SparseSet<int>& selectedRows)
{
    deselectAll();
    for (auto i = 0; i < selectedRows.size(); i++)
    {
        if (auto region = getRegion(selectedRows[i]))
        {
            region->setSelected(true);
        }
    }
}

void AudioRegionContainer::setSelectedPosition(juce::Range<double> pos, audium::TimeContextType context)
{
    if (context == audium::seconds)
    {
        selectedPositionClocks = playListScheduler->getTempoProvider()->secondsToClocks(pos);
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

juce::Range<double> AudioRegionContainer::getSelectedPosition(audium::TimeContextType context) const
{
    if (context == audium::seconds)
    {
        return playListScheduler->getTempoProvider()->clocksToSeconds(selectedPositionClocks);
    }
    else if (context == audium::clocks)
    {
        return selectedPositionClocks;
    }

    jassertfalse;
    return juce::Range<double>();
}

std::shared_ptr<AudioRegion> AudioRegionContainer::getRegion(int rowNumber) const
{
    if (rowNumber >= 0 && rowNumber < audioRegions.size())
    {
        return audioRegions[rowNumber];
    }
    return nullptr;
}

int AudioRegionContainer::getRegionIndex(std::shared_ptr<AudioRegion> searchRegion) const
{
    auto it = std::find(audioRegions.begin(), audioRegions.end(), searchRegion);
    if (it == audioRegions.end())
    {
        jassertfalse;
        return -1; // not found
    }
    else
    {
        auto index = std::distance(audioRegions.begin(), it);
        return static_cast<int>(index);
    }
}

int AudioRegionContainer::getNumRegions(const AudioGroup* group) const
{
    if (group == nullptr)
    {
        return static_cast<int>(audioRegions.size());
    }
    else
    {
        int count = 0;
        for (auto region : audioRegions)
        {
            if (region->getAudioGroup().get() == group)
                count++;
        }
        return count;
    }
}

void AudioRegionContainer::setRegionName(int rowNumber, juce::String newName)
{
    if (AudioRegion* r = getRegion(rowNumber).get())
    {
        r->setName(newName);
    }
}

void AudioRegionContainer::setRegionStart(int rowNumber, double newStart)
{
    if (AudioRegion* r = getRegion(rowNumber).get())
    {
        r->setRegionStart(newStart, audium::seconds);
    }
    
    sendActionMessage (regionStartAction);
}

void AudioRegionContainer::setRegionEnd(int rowNumber, double newEnd)
{
    if (AudioRegion* r = getRegion(rowNumber).get())
    {
        r->setRegionEnd(newEnd, audium::seconds);
    }
    sendActionMessage (regionEndAction);
}

void AudioRegionContainer::setRegionLength(int rowNumber, double newLength)
{
    if (AudioRegion* r = getRegion(rowNumber).get())
    {
        r->setRegionLength(newLength, audium::seconds);
    }
    
    sendActionMessage (regionLengthAction);
}

std::vector<std::shared_ptr<AudioRegion>> AudioRegionContainer::getRegionsForGroup(std::shared_ptr<AudioGroup> group) const
{
    std::vector<std::shared_ptr<AudioRegion>> regions;
    for (auto region : audioRegions)
    {
        if (region->getAudioGroup() == group)
            regions.push_back(region);
    }
    return regions;
}

std::vector<std::shared_ptr<AudioRegion>> AudioRegionContainer::getRegionsForSubGroup(const AudioSubGroup* subGroup) const
{
    std::vector<std::shared_ptr<AudioRegion>> regions;
    for (auto region : audioRegions)
    {
        if (region->getAudioSubGroup().get() == subGroup)
            regions.push_back(region);
    }
    return regions;
}

void AudioRegionContainer::deleteAudioRegionsForGroup(std::shared_ptr<AudioGroup> group)
{
    auto regions = getRegionsForGroup(group);
    
    for (auto region : regions)
    {
        deleteAudioRegion(region);
    }
}

void AudioRegionContainer::deleteAudioRegionsForSubGroup(std::shared_ptr<AudioSubGroup> audioSubGroup)
{
    auto regions = getRegionsForSubGroup(audioSubGroup.get());
    
    for (auto region : regions)
    {
        deleteAudioRegion(region);
    }
}

void AudioRegionContainer::deleteAudioRegion(std::shared_ptr<AudioRegion> region)
{
    region->getAudioGroup()->getPlayListContainer()->deleteAssociatedItems(region);

    auto atIndex = getRegionIndex(region);
    jassert(atIndex >= 0 && atIndex < audioRegions.size());
    audioRegions.erase(audioRegions.begin() + atIndex);
}

std::vector<std::shared_ptr<AudioRegion>> AudioRegionContainer::getRegionsForResource(std::shared_ptr<AudioResource> audioResource) const
{
    std::vector<std::shared_ptr<AudioRegion>> result;
    for (auto region : audioRegions)
    {
        auto audioResources = region->getAudioResources();
        for (auto resource : audioResources)
        {
            if (resource == audioResource)
            {
                result.push_back(region);
            }
        }
    }
    return result;
}
