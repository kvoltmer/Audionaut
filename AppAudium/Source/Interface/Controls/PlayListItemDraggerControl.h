/*
  ==============================================================================

    PlayListItemDraggerControl.h
    Created: 27 Dec 2023 4:52:45pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include "DraggerControl.h"
#include "Engine/PlayList/PlayListItem.h"

class PlayListItemDraggerControl : public DraggerControl
{
public:
    
    PlayListItemDraggerControl(juce::Component* componentToDrag,
                               std::shared_ptr<AudiumEngine> audiumEngine,
                               std::shared_ptr<PlayListItem> playListItem,
                               std::shared_ptr<ZoomHandler> zoomHandler,
                               juce::Colour colour,
                               std::shared_ptr<RegionSelector> regionSelector) :
        DraggerControl(componentToDrag, audiumEngine, zoomHandler, colour, regionSelector),
        playListItem(playListItem)
    {
    }

    ~PlayListItemDraggerControl() override
    {
    }
    
    void setRegionDataInSeconds(const juce::Range<double> newRegionData) override
    {
        // TODO: implement
    }
    
    
    bool isSelected() const override
    {
        return playListItem->isSelected();
    }
    
    void setSelected(bool bSelected, bool deselectOthers) override
    {
        if (deselectOthers)
            playListItem->getRegion()->getAudioGroup()->getPlayListContainer()->deselectAll();
        playListItem->setSelected(bSelected);
        playListItem->getPlayListContainer().sendActionMessage(playListItemSelection);
    }
    
    const juce::String getLabelString() const override
    {
        return playListItem->getRegion()->getName();
    }
    
    bool validateData() override
    {
        return true;
//        const auto audioResources = audioSubGroup->getAudioResources();
//        bool result = false;
//        for (auto res : audioResources)
//        {
//            result |= res->validateData();
//        }
//        return result;
    }
    
    bool keyPressed (const KeyPress& key, Component* originatingComponent) override
    {
        if (key.isKeyCode (KeyPress::deleteKey) || key.isKeyCode (KeyPress::backspaceKey))
        {
            playListItem->getRegion()->getAudioGroup()->getPlayListContainer()->deleteSelectedItems();
            return true;
        }
        
        return false;
    }
    
    
    
private:
    std::shared_ptr<PlayListItem> playListItem;
};
