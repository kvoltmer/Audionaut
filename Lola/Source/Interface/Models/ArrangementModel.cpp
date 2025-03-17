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

#include "ArrangementModel.h"
#include "Interface/Components/MiddlePanel/ArrangementView/PlayListItemComponent.h"

#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/Group/AudioTrack.h"

ArrangementModel::ArrangementModel(std::shared_ptr<audium::AudiumEngine>   audiumEngine_,
                                   std::shared_ptr<audium::AudioTrack>     audioTrack_,
                                   std::shared_ptr<RegionSelector> regionSelector_,
                                   std::shared_ptr<ZoomHandler>    zoomHandler_) :
    audiumEngine(audiumEngine_),
    audioTrack(audioTrack_),
    regionSelector(regionSelector_),
    zoomHandler(zoomHandler_)
{
}

int ArrangementModel::getNumRows() {
    return audioTrack->getPlayListContainer()->getNumItems();
}

juce::Component* ArrangementModel::refreshComponentForItem (int itemNumber, juce::Component* existingComponentToUpdate)
{
    if (audioTrack->getPlayListContainer()->playListItems.objectExistsAtIndex(itemNumber)) {
        auto playListItem = audioTrack->getPlayListContainer()->playListItems.getObjects()[itemNumber];
        
        if (existingComponentToUpdate == nullptr) {
            auto playListItemComponent = new PlayListItemComponent(audiumEngine,
                                                                   audioTrack,
                                                                   audioTrack->getPlayListContainer(),
                                                                   zoomHandler,
                                                                   regionSelector);
            playListItemComponent->setPlayListItem(playListItem);
            return playListItemComponent;
        }
        else {
            auto component = dynamic_cast<PlayListItemComponent*>(existingComponentToUpdate);
            jassert(component);
            component->updateUI(playListItem);
            return component;
        }
    }
    
    return nullptr;
}

const juce::Range<double> ArrangementModel::getRangeForItem(int itemNumber) const
{
    juce::Range<double> result;
    if (audioTrack->getPlayListContainer()->playListItems.objectExistsAtIndex(itemNumber)) {
        auto playListItem = audioTrack->getPlayListContainer()->playListItems.getObjects()[itemNumber];
        auto start = zoomHandler->clocksToX(playListItem->getAbsolutePosition(audium::clocks));
        auto width = zoomHandler->clocksToX(playListItem->getDurationTime(audium::clocks));
        result.setStart(start);
        result.setLength(width);
    }
    
    return result;
    
}
