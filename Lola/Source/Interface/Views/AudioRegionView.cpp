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
    return playListItem->getRegion()->getGain(channelNumber);
}

void AudioRegionView::resized()
{
    fadeInOutView->setBounds(getLocalBounds());
    
    auto sliderWidth = 67;
    auto sliderHeight = 15;
    auto space = 5;
    
    if (getWidth() > sliderWidth + (space * 2)) {
        volumeSlider->setVisible(true);
        volumeSlider->setBounds (space,
                                 getHeight() - sliderHeight - space,
                                 sliderWidth,
                                 sliderHeight);
    }
    else {
        volumeSlider->setVisible(false);
    }
}

void AudioRegionView::updateUI(int theChannel)
{
    channelNumber = theChannel;
        
    volumeSlider->setValue(LevelMeter::gainToDecebel(getClipGain()), dontSendNotification);
}


