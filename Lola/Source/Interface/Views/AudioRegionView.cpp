//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Lola uses a GPL/commercial licence - see LICENCE.md for details.

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
    return playListItem->getRegion()->getRegionData(audium::seconds).getStart();
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
    //std::cout << "AudioRegionView::updateUI " << playListItem->getRegion()->getName() << std::endl;
    channelNumber = theChannel;
        
    volumeSlider->setValue(LevelMeter::gainToDecebel(getClipGain()), dontSendNotification);
}

void AudioRegionView::setPlayListItem(std::shared_ptr<audium::PlayListItem> item)
{
    playListItem = item;
    
    fadeInOutView->setPlayListItem(item);
    
    auto audioRegion = item->getRegion();
    
    volumeSlider->onValueChange = [this, audioRegion] {
        audioRegion->setGain(channelNumber, Decibels::decibelsToGain(volumeSlider->getValue()), true);
        this->audiumEngine->getAudioTrackContainer()->sendActionMessage(audium::updateArrangementAction);
    };
    volumeSlider->onDragStart = [this] {
        playListItem->onDragStart();
    };
    
    volumeSlider->onDragEnd = [this] {
        playListItem->onDragEnd();
    };
}


