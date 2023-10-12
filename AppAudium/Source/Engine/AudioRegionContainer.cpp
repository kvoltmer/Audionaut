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
#include "Engine/PlayList/PlayListContainer.h"

std::shared_ptr<AudioRegion> AudioRegionContainer::createDefaultRegion(std::shared_ptr<AudioResourceContainer> audioResourceContainer,
                                                                       std::shared_ptr<AudioGroup> group)
{
    jassert(getNumRegions(group.get()) == 0);
    auto audioResources = audioResourceContainer->getAudioResourcesForGroup(group.get());
    jassert(audioResources.size() > 0);
    auto name = audioResources[0]->getFileNameWithoutExtension();
    auto totalLength = audioResourceContainer->getTotalLengthMax();
    return createRegion(name, juce::Range(0.0, totalLength), group);
}

std::shared_ptr<AudioRegion> AudioRegionContainer::createRegion(juce::String regionName, juce::Range<double> position, std::shared_ptr<AudioGroup> group)
{
    if (group == nullptr)
    {
        group = audioResourceContainer->getDefaultGroup();
    }
    
    auto audioRegion = std::shared_ptr<AudioRegion>(new AudioRegion(group));
    audioRegion->position = position;
    audioRegion->name = regionName;
    audioRegions.push_back(audioRegion);
    // don't select by default
    // selectedRowNumber = static_cast<int>(audioRegions.size() - 1);
    sendActionMessage(regionCreatedAction);
    return audioRegion;
}

void AudioRegionContainer::deleteRegion(int atIndex)
{
    auto region = getRegion(atIndex);
    region->getAudioGroup()->getPlayListContainer()->deleteAssociatedItems(region);
    
    clearSelection();
    if (atIndex >= 0 && atIndex < audioRegions.size())
    {
        audioRegions.erase(audioRegions.begin() + atIndex);
        sendActionMessage(regionDeletedAction);
    }
}

void AudioRegionContainer::setRegionPosition(juce::Range<double> pos)
{
    selectedRegion = pos;
    if (selectedRowNumber >= 0)
    {
        getRegion(selectedRowNumber)->position = pos;
    }
    
    //std::cout << "start " << pos.getStart() << " end " << pos.getEnd() << std::endl;
    sendActionMessage(regionModifiedAction);
}

void AudioRegionContainer::clearSelection()
{
    selectedRowNumber = -1;
    //selectedRegion.position = juce::Range<double>();
    sendActionMessage(regionClearedAction);
}

juce::Range<double> AudioRegionContainer::getRegionPosition() const
{
    return selectedRegion;
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
    if (const AudioRegion* const r = getRegion(rowNumber).get())
    {
        selectedRegion = r->position;
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
        outputStream.writeString(region->name);
        outputStream.writeDouble(region->position.getStart());
        outputStream.writeDouble(region->position.getEnd());
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
            auto regionName = inputStream.readString();
            auto start = inputStream.readDouble();
            auto end = inputStream.readDouble();
            juce::Range<double> position(start, end);
            createRegion(regionName, position);
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
        if (newStart <=  r->position.getEnd())
        {
            r->position.setStart(newStart);
        }
    }
//    if (rowNumber == selectedRowNumber)
//    {
//        selectedRegion.position.setLength(newLength);
//    }
    
    sendActionMessage (regionStartAction);
}

void AudioRegionContainer::setRegionEnd(int rowNumber, double newEnd)
{
    if (AudioRegion* r = getRegion(rowNumber).get())
    {
        if (newEnd >=  r->position.getStart())
        {
            r->position.setEnd(newEnd);
        }
    }
    sendActionMessage (regionEndAction);
}

void AudioRegionContainer::setRegionLength(int rowNumber, double newLength)
{
    if (AudioRegion* r = getRegion(rowNumber).get())
    {
        r->position.setLength(newLength);
    }
//    if (rowNumber == selectedRowNumber)
//    {
//        selectedRegion.position.setLength(newLength);
//    }
    
    sendActionMessage (regionLengthAction);
}

void AudioRegionContainer::removeAudioRegionsForGroup(std::shared_ptr<AudioGroup> group)
{
    std::vector< std::shared_ptr<AudioRegion> >::reverse_iterator i = audioRegions.rbegin();
    
    while ( i != audioRegions.rend() )
    {
        if ( (*i)->getAudioGroup() == group )
        {
            i = decltype(i)(audioRegions.erase( std::next(i).base() ));
        }
        else
        {
            ++i;
        }
    }
}
