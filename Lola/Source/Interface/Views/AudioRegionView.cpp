/*
  ==============================================================================

 AudioRegionView.cpp
    Created: 19 Sep 2023 2:20:32pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include <JuceHeader.h>
#include "AudioRegionView.h"
#include "Interface/Handlers/ZoomHandler.h"
#include "Engine/Resource/AudioResource.h"
#include "Interface/ColourIds.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/Region/AudioRegion.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/PlayList/PlayListScheduler.h"

using namespace juce;

double AudioRegionView::getRegionStart(audium::TimeContextType context) const
{
    return audioRegion->getRegionData(audium::seconds).getStart();
}

double AudioRegionView::getClipGain() const
{
    return playListItem->getGain();
}
