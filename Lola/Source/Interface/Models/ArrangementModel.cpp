/*
  ==============================================================================

    ArrangementModel.cpp
    Created: 14 Mar 2025 10:50:54am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "ArrangementModel.h"
#include "Interface/Components/MiddlePanel/ArrangementView/PlayListItemComponent.h"

#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/Group/AudioTrack.h"

ArrangementModel::ArrangementModel(std::shared_ptr<AudiumEngine>   audiumEngine_,
                                   std::shared_ptr<AudioTrack>     audioTrack_,
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
