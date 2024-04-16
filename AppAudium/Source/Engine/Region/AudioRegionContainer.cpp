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

std::shared_ptr<AudioRegion> AudioRegionContainer::createRegion(std::shared_ptr<AudioGroup> group,
                                                                std::shared_ptr<AudioSubGroup> subGroup)
{
    jassert(group != nullptr);
    jassert(subGroup != nullptr);
    auto audioRegion = std::shared_ptr<AudioRegion>(new AudioRegion(group, subGroup, tempoProvider));
    audioRegions.push_back(audioRegion);
    return audioRegion;
}

std::shared_ptr<AudioRegion> AudioRegionContainer::createRegion(juce::String regionName,
                                                                juce::Range<double> position,
                                                                std::shared_ptr<AudioGroup> group,
                                                                std::shared_ptr<AudioSubGroup> subGroup)
{
    jassert(group != nullptr);
    
    if (subGroup == nullptr)
    {
        subGroup = group->getDefaultSubGroup();
    }
    
    auto audioRegion = createRegion(group, subGroup);
    audioRegion->setRegionData(position, audium::seconds);
    audioRegion->setName(regionName);
    audioGroupContainer.sendActionMessage(regionCreatedAction);
    return audioRegion;
}

void AudioRegionContainer::cleanup()
{
    for (auto region : audioRegions)
    {
        region->deleteAssociatedItems();
    }
    
    audioRegions.clear();
}

void AudioRegionContainer::deleteRegion(int atIndex)
{
    auto region = getRegion(atIndex);
    region->deleteAssociatedItems();
    
    if (atIndex >= 0 && atIndex < audioRegions.size())
    {
        audioRegions.erase(audioRegions.begin() + atIndex);
        audioGroupContainer.sendActionMessage(regionDeletedAction);
    }
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
    region->deleteAssociatedItems();
    
    auto atIndex = getRegionIndex(region);
    if (atIndex >= 0 && atIndex < audioRegions.size())
    {
        audioRegions.erase(audioRegions.begin() + atIndex);
    }
    else
    {
        jassertfalse;
    }
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
