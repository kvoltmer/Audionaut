/*
  ==============================================================================

    AudioRegionContainer.cpp
    Created: 30 May 2023 10:16:35am
    Author:  Klaus Voltmer

  ==============================================================================
*/
#include <iostream>

#include "AudioRegionContainer.h"

void AudioRegionContainer::createRegion(juce::String regionName, juce::Range<double> position)
{
    auto audioRegion = std::shared_ptr<AudioRegion>(new AudioRegion());
    audioRegion->position = position;
    audioRegion->name = regionName;
    audioRegions.push_back(audioRegion);
    selectedRowNumber = static_cast<int>(audioRegions.size() - 1);
    sendActionMessage(regionCreatedAction);
}

void AudioRegionContainer::deleteRegion(int atIndex)
{
    clearSelection();
    if (atIndex >= 0 && atIndex < audioRegions.size())
    {
        audioRegions.erase(audioRegions.begin() + atIndex);
        sendActionMessage(regionDeletedAction);
    }
}

void AudioRegionContainer::setRegionPosition(juce::Range<double> pos)
{
    selectedRegion.position = pos;
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
    return selectedRegion.position;
}

std::shared_ptr<AudioRegion> AudioRegionContainer::getRegion(int rowNumber) const
{
    if (rowNumber >= 0 && rowNumber < audioRegions.size())
    {
        return audioRegions[rowNumber];
    }
    return nullptr;
}

void AudioRegionContainer::setSelectedRegion(int rowNumber)
{
    if (const AudioRegion* const r = getRegion(rowNumber).get())
    {
        selectedRegion = *r;
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
        outputStream.writeInt(region->position.getStart());
        outputStream.writeInt(region->position.getEnd());
    }
    return true;
}

bool AudioRegionContainer::readFromStream (juce::InputStream& inputStream)
{
    if (!inputStream.isExhausted())
    {
        audioRegions.clear();
        auto numRegions = inputStream.readInt();
        for (auto i = 0; i < numRegions; i++)
        {
            auto regionName = inputStream.readString();
            auto start = inputStream.readInt();
            auto end = inputStream.readInt();
            juce::Range<double> position(start, end);
            createRegion(regionName, position);
        }
    }
    return true;
}
