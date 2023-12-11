/*
  ==============================================================================

    AudioRegionContainer.cpp
    Created: 30 May 2023 10:16:35am
    Author:  Klaus Voltmer

  ==============================================================================
*/
#include <iostream>

#include "AudioRegionContainer.h"
#include "Engine/ActionMessages.h"
#include "Engine/AudioResourceContainer.h"
#include "Engine/AudioGroupContainer.h"
#include "Engine/AudioGroup.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/AudiumTransportSource.h"

std::shared_ptr<AudioRegion> AudioRegionContainer::createDefaultRegion(std::shared_ptr<AudioGroup> group)
{
    jassert(getNumRegions(group.get()) == 0);
    auto audioResources = audioResourceContainer->getAudioResourcesForGroup(group.get());
    auto name = (audioResources.size() > 0) ? audioResources[0]->getFileNameWithoutExtension() : "n/a";
    auto seconds = 0.0;
    for (auto resource : audioResources)
    {
        seconds = juce::jmax(seconds, resource->getAudioTransportSource()->getLengthInSeconds());
    }
    jassert(seconds > 0.0);
    
    // TODO: provide a audio resource pointer
    std::shared_ptr<AudioResource> audioResource = nullptr;
    jassertfalse;
    
    return createRegion(name, juce::Range(0.0, seconds), group, audioResource);
}

void AudioRegionContainer::createRegionsFromSelection(juce::String name)
{
    for (auto i = 0; i < audioGroupContainer->getNumItems(); i++)
    {
        if (auto group = audioGroupContainer->getAudioGroup(i))
        {
            if (playListScheduler->isArrangementMode())
            {
                if (auto item = group->getPlayListContainer()->itemAtAbsoluteRange(selectedPositionClocks))
                {
                    // we need the start of the actual audio file
                    auto localStart = selectedPositionClocks.getStart() - item->getAbsolueStartTime() + item->getRegionDataInClocks().getStart();
                    
                    juce::Range<double> localRange(localStart, localStart + selectedPositionClocks.getLength());
                    auto localRangeInSeconds = playListScheduler->getTempoProvider()->clocksToSeconds(localRange);
                    createRegion(name, localRangeInSeconds, group, item->getRegion()->getAudioResource());
                }
            }
            else
            {
                // get resources at this range
                
                auto rangeInSeconds = playListScheduler->getTempoProvider()->clocksToSeconds(selectedPositionClocks);
                auto resources = group->getAudioResourcesAtAbsoluteRange(rangeInSeconds);
                for (auto resource : resources)
                {
                    const auto startPosition = resource->getTransportPositionSeconds();
                    rangeInSeconds -= startPosition;
                    createRegion(name, rangeInSeconds, group, resource);
                }
            }
        }
    }
    // clear selection
    selectedPositionClocks = juce::Range<double>();
}

std::shared_ptr<AudioRegion> AudioRegionContainer::createRegion(juce::String regionName,
                                                                juce::Range<double> position,
                                                                std::shared_ptr<AudioGroup> group,
                                                                std::shared_ptr<AudioResource> audioResource)
{
    if (group == nullptr)
    {
        group = audioGroupContainer->getDefaultGroup();
    }
    // TODO: validate position data
    //jassert(audioResource->getRegionDataInSeconds().contains(position));
    
    auto audioRegion = std::shared_ptr<AudioRegion>(new AudioRegion(group, audioResource, playListScheduler->getTempoProvider()));
    audioRegion->setRegionDataInSeconds(position);
    audioRegion->name = regionName;
    audioRegions.push_back(audioRegion);
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

void AudioRegionContainer::deselectAll()
{
    for (auto & region : audioRegions)
    {
        region->setSelected(false);
    }
    sendActionMessage("todo");
}

void AudioRegionContainer::setSelectedPositionInSeconds(juce::Range<double> pos)
{
    selectedPositionClocks = playListScheduler->getTempoProvider()->secondsToClocks(pos);
}

juce::Range<double> AudioRegionContainer::getSelectedPositionInSeconds() const
{
    return playListScheduler->getTempoProvider()->clocksToSeconds(selectedPositionClocks);
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
        return -1; // not found
    }
    else
    {
        auto index = std::distance(audioRegions.begin(), it);
        return static_cast<int>(index);
    }
    
}

void AudioRegionContainer::setSelectedRegion(int rowNumber)
{
    if (AudioRegion* r = getRegion(rowNumber).get())
    {
        r->setSelected(true);
        selectedRowNumber = rowNumber;
        sendActionMessage (regionSelectedAction);
    }
}

int AudioRegionContainer::getSelectedRegion() const
{
    return selectedRowNumber;
}

bool AudioRegionContainer::writeToStream (juce::OutputStream& outputStream)
{
    outputStream.writeInt((int)audioRegions.size());
    for (auto & region : audioRegions)
    {
        outputStream.writeInt(region->getAudioGroup()->getId());
        outputStream.writeString(region->name);
        outputStream.writeDouble(region->getRegionDataInSeconds().getStart());
        outputStream.writeDouble(region->getRegionDataInSeconds().getEnd());
        outputStream.writeInt(region->getAudioResource()->getId());
    }
    return true;
}

bool AudioRegionContainer::readFromStream (juce::InputStream& inputStream)
{
    jassert(audioRegions.empty());
    
    if (!inputStream.isExhausted())
    {
        auto numRegions = inputStream.readInt();
        for (auto i = 0; i < numRegions; i++)
        {
            auto groupId = inputStream.readInt();
            auto regionName = inputStream.readString();
            auto start = inputStream.readDouble();
            auto end = inputStream.readDouble();
            
            juce::Range<double> position(start, end);
            auto id = inputStream.readInt();
            auto audioResource = audioResourceContainer->getAudioResourceById(id);
            if (audioResource != nullptr)
            {
                createRegion(regionName, position, audioGroupContainer->getAudioGroupById(groupId), audioResource);
            }
        }
    }
    return true;
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
        r->name = newName;
    }
}

void AudioRegionContainer::setRegionStart(int rowNumber, double newStart)
{
    if (AudioRegion* r = getRegion(rowNumber).get())
    {
        r->setRegionStart(newStart);
    }
    
    sendActionMessage (regionStartAction);
}

void AudioRegionContainer::setRegionEnd(int rowNumber, double newEnd)
{
    if (AudioRegion* r = getRegion(rowNumber).get())
    {
        r->setRegionEnd(newEnd);
    }
    sendActionMessage (regionEndAction);
}

void AudioRegionContainer::setRegionLength(int rowNumber, double newLength)
{
    if (AudioRegion* r = getRegion(rowNumber).get())
    {
        r->setRegionLength(newLength);
    }
    
    sendActionMessage (regionLengthAction);
}

std::vector<std::shared_ptr<AudioRegion>> AudioRegionContainer::getRegionsForGroup(std::shared_ptr<AudioGroup> group) const
{
    jassertfalse;
    std::vector<std::shared_ptr<AudioRegion>> regions;
    for (auto region : audioRegions)
    {
        if (region->getAudioGroup() == group)
            regions.push_back(region);
    }
    return regions;
}

void AudioRegionContainer::removeAudioRegionsForGroup(std::shared_ptr<AudioGroup> group)
{
    auto regions = getRegionsForGroup(group);
    
    for (auto region : regions)
    {
        removeAudioRegion(region);
    }
}

void AudioRegionContainer::removeAudioRegion(std::shared_ptr<AudioRegion> region)
{
    auto atIndex = getRegionIndex(region);
    jassert(atIndex >= 0 && atIndex < audioRegions.size());
    audioRegions.erase(audioRegions.begin() + atIndex);
}

std::vector<std::shared_ptr<AudioRegion>> AudioRegionContainer::getRegionsForResource(std::shared_ptr<AudioResource> audioResource) const
{
    std::vector<std::shared_ptr<AudioRegion>> result;
    for (auto region : audioRegions)
    {
        if (region->getAudioResource() == audioResource)
            result.push_back(region);
    }
    return result;
}

void AudioRegionContainer::copyRegionsForResource(std::shared_ptr<AudioResource> audioResource)
{
    auto resources = audioResource->getEqualAudioResources();
    if (resources.size() > 0)
    {
        auto regions = getRegionsForResource(resources[0]);
        for (auto region : regions)
        {
            createRegion(region->name, region->getRegionDataInSeconds(), region->getAudioGroup(), audioResource);
        }
    }
}
