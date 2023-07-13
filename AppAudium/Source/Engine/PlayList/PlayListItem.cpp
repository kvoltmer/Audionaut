/*
  ==============================================================================

    PlayListItem.cpp
    Created: 28 Jun 2023 11:51:10am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "PlayListItem.h"
#include "Engine/AudioRegion.h"

PlayListItem::PlayListItem(std::shared_ptr<AudioRegion> audioRegion) :
    audioRegion(audioRegion)
{
}

juce::Range<double> PlayListItem::getRegionData() const
{
    return audioRegion->position;
}
