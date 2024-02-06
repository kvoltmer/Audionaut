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

juce::Range<double> PlayListItem::getRegionData(audium::TimeContextType context) const
{
    return audioRegion->getRegionData(context);
}

double PlayListItem::getAbsolueStartTime(audium::TimeContextType context) const
{
    return owner.getAbsolueStartTime(this, context);
}

double PlayListItem::getDurationTime(audium::TimeContextType context) const
{
    return getRegionData(context).getLength();
}

juce::Range<double> PlayListItem::getAbsolutePositionRange(audium::TimeContextType context) const
{
    const auto start = getAbsolueStartTime(context);
    const auto length = getRegionData(context).getLength();
    return juce::Range<double>(start, start + length);
}

double PlayListItem::getAbsolutePosition(audium::TimeContextType context) const
{
    return getAbsolueStartTime(context);
}

void PlayListItem::setAbsolutePosition(double position, audium::TimeContextType context)
{
    /// TODO: implement...
}
