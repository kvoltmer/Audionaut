//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.

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

void AudioRegionView::setPlayListItem(std::shared_ptr<PlayListItem> item)
{
    playListItem = item;
    
    fadeInOutView->setPlayListItem(item);
    
    auto audioRegion = item->getRegion();
    
    volumeSlider->onValueChange = [this, audioRegion] {
        audioRegion->setGain(channelNumber, Decibels::decibelsToGain(volumeSlider->getValue()), true);
        this->audiumEngine->getAudioTrackContainer()->sendActionMessage(updateArrangementAction);
    };
    volumeSlider->onDragStart = [this] {
        playListItem->onDragStart();
    };
    
    volumeSlider->onDragEnd = [this] {
        playListItem->onDragEnd();
    };
}


