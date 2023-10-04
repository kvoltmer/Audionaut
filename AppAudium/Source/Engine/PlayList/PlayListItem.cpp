/*
  ==============================================================================

    PlayListItem.cpp
    Created: 28 Jun 2023 11:51:10am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "PlayListItem.h"
#include "Engine/AudioRegion.h"
#include "Engine/PlayList/PlayListContainer.h"

PlayListItem::PlayListItem(const PlayListContainer &owner, std::shared_ptr<AudioRegion> audioRegion) :
    owner(owner),
    audioRegion(audioRegion)
{
}

juce::Range<double> PlayListItem::getRegionData() const
{
    return audioRegion->position;
}

double PlayListItem::getStartTime() const
{
    return owner.getItemStartTime(this);
}

double PlayListItem::getDurationTime() const
{
    return getRegionData().getLength();
}
