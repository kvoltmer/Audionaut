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
    
    PlayListItemDraggerControl(juce::Component* componentToDrag,
                               std::shared_ptr<AudiumEngine> audiumEngine,
                               std::shared_ptr<PlayListContainer> playListContainer,
                               std::shared_ptr<PlayListItem> playListItem,
                               std::shared_ptr<ZoomHandler> zoomHandler,
                               juce::Colour colour,
                               std::shared_ptr<RegionSelector> regionSelector) :
        DraggerControl(componentToDrag, audiumEngine, zoomHandler, colour, regionSelector),
        playListContainer(playListContainer),
        playListItem(playListItem)
    {
    }

    ~PlayListItemDraggerControl() override
    {
    }
    
    void commitData(const juce::Range<double> newData, audium::TimeContextType context) override;
        
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
    
    const juce::String getLabelString() const override
    {
        return playListItem->getRegion()->getName();
    }
    
    bool validateData() override;
    
private:
    std::shared_ptr<PlayListContainer> playListContainer;
    std::shared_ptr<PlayListItem> playListItem;
    
};
