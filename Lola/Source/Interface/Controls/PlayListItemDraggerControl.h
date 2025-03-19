//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Lola uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include "Interface/Controls/DraggerControl.h"

#include "Engine/PlayList/PlayListItem.h"
#include "Engine/PlayList/PlayListContainer.h"

class PlayListItemDraggerControl : public DraggerControl
{
public:
    
    PlayListItemDraggerControl(std::shared_ptr<audium::AudiumEngine> audiumEngine_,
                               std::shared_ptr<audium::PlayListContainer> playListContainer_,
                               std::shared_ptr<ZoomHandler> zoomHandler_,
                               juce::Colour colour_,
                               std::shared_ptr<RegionSelector> regionSelector_) :
        DraggerControl(audiumEngine_,
                       zoomHandler_,
                       colour_,
                       regionSelector_),
        playListContainer(playListContainer_)
    {
        regionSelector->playListItemDraggerControls.push_back(this);
    }

    ~PlayListItemDraggerControl() override
    {
        std::erase_if(regionSelector->playListItemDraggerControls, [this](const auto* item) {
            return item == this;
        });
    }
        
    bool isSelected() const override
    {
        return playListItem->isSelected();
    }
    
    void setSelected(bool bSelected, bool deselectOthers) override
    {
        if (deselectOthers)
        {
            audiumEngine->getAudioTrackContainer()->getSelectionManager()->deselectAll();
        }
        playListItem->setSelected(bSelected);
        audiumEngine->getAudioTrackContainer()->sendActionMessage(audium::updateSelection);
    }
    
    void shiftSelect() override;
    
    const juce::String getLabelString() const override
    {
        return playListItem->getRegion()->getName();
    }
    
    bool validateData() override;
    
    void setPlayListItem(std::shared_ptr<audium::PlayListItem> playListItem_) { playListItem = playListItem_; }
    
private:
    std::shared_ptr<audium::PlayListContainer> playListContainer;
    std::shared_ptr<audium::PlayListItem> playListItem;
    
};
