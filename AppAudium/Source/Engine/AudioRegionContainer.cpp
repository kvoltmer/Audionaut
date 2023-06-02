/*
  ==============================================================================

    AudioRegionContainer.cpp
    Created: 30 May 2023 10:16:35am
    Author:  Klaus Voltmer

  ==============================================================================
*/
#include <iostream>

#include "AudioRegionContainer.h"

std::shared_ptr<AudioRegion> AudioRegionContainer::createRegion(juce::String regionName)
{
    auto audioRegion = std::shared_ptr<AudioRegion>(new AudioRegion());
    audioRegion->position = selectedRegion.position;
    audioRegion->name = regionName;
    audioRegions.push_back(audioRegion);
    //std::cout << "region created" << std::endl;
    return audioRegion;
}

void AudioRegionContainer::setSelectedRegion(juce::Range<double> pos)
{
    selectedRegion.position = pos;
    
    std::cout << "start " << pos.getStart() << " end " << pos.getEnd() << std::endl;
}

juce::Range<double> AudioRegionContainer::getSelectedRegion() const
{
    return selectedRegion.position;
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
            auto region = createRegion(inputStream.readString());
            region->position.setStart(inputStream.readInt());
            region->position.setEnd(inputStream.readInt());
        }
    }
    return true;
}
