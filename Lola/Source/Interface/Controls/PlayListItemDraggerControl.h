/*
  ==============================================================================

    PlayListItemDraggerControl.h
    Created: 27 Dec 2023 4:52:45pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include "Interface/Controls/DraggerControl.h"

#include "Engine/PlayList/PlayListItem.h"
#include "Engine/PlayList/PlayListContainer.h"

class PlayListItemDraggerControl : public DraggerControl
{
public:
    
    PlayListItemDraggerControl(juce::Component* componentToDrag_,
                               std::shared_ptr<AudiumEngine> audiumEngine_,
                               std::shared_ptr<PlayListContainer> playListContainer_,
                               std::shared_ptr<PlayListItem> playListItem_,
                               std::shared_ptr<ZoomHandler> zoomHandler_,
                               juce::Colour colour_,
                               std::shared_ptr<RegionSelector> regionSelector_) :
        DraggerControl(componentToDrag_,
                       audiumEngine_,
                       zoomHandler_,
                       colour_,
                       regionSelector_,
                       std::static_pointer_cast<PositionableBase>(playListItem_)),
        playListContainer(playListContainer_),
        playListItem(playListItem_)
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
        audiumEngine->getAudioTrackContainer()->sendActionMessage(updateSelection);
    }
    
    void shiftSelect() override;
    
    const juce::String getLabelString() const override
    {
        return playListItem->getRegion()->getName();
    }
    
    bool validateData() override;
    
private:
    std::shared_ptr<PlayListContainer> playListContainer;
    std::shared_ptr<PlayListItem> playListItem;
    
};
