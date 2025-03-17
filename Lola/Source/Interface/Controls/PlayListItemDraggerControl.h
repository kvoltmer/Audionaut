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
