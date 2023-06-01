/*
  ==============================================================================

    AudioRegionContainer.cpp
    Created: 30 May 2023 10:16:35am
    Author:  Klaus Voltmer

  ==============================================================================
*/
#include <iostream>

#include "AudioRegionContainer.h"

void AudioRegionContainer::createRegion(juce::String regionName)
{
    auto audioRegion = std::shared_ptr<AudioRegion>(new AudioRegion());
    audioRegion->position = selectedRegion.position;
    audioRegion->name = regionName;
    audioRegions.push_back(audioRegion);
    
    std::cout << "region created" << std::endl;
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
