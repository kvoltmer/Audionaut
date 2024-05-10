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
    
    void commitData(const juce::Range<double> newData, audium::TimeContextType context) override
    {
        // undo
        if (undoableContainerAction == nullptr)
        {
            //std::cout << "new audium::UndoableContainerAction" << std::endl;
            undoableContainerAction = new audium::UndoableContainerAction(*audiumEngine->getAudioGroupContainer(), false);
        }
        
        switch (currentDragMode)
        {
            case leftEdge:
                break;
            case rightEdge:
                break;
            case middleEdge:
                // position in transport
                playListItem->setAbsolutePosition(newData.getStart(), context);
                break;
            default:
                break;
        }
        
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
        bool result = playListItem->validateData();
        
        if (undoableContainerAction != nullptr)
        {
            // Undo: store new state
            undoableContainerAction->storeNewState();
            audiumEngine->getUndoManager()->perform(undoableContainerAction, "Modify Item");
            audiumEngine->getUndoManager()->beginNewTransaction();
            undoableContainerAction = nullptr;
            //std::cout << "undoableContainerAction = nullptr" << std::endl;
        }
        
        return result;
    }
    
    bool keyPressed (const KeyPress& key, Component* originatingComponent) override
    {
        if (key.isKeyCode (KeyPress::deleteKey) || key.isKeyCode (KeyPress::backspaceKey))
        {
            // Undo: store old state
            auto action = std::make_unique<audium::UndoableContainerAction>(*audiumEngine->getAudioGroupContainer());

            playListItem->getRegion()->getAudioGroup()->getPlayListContainer()->deleteSelectedItems();
            
            
            // Undo: store new state and perform
            action->storeNewState();
            audiumEngine->getUndoManager()->perform(action.release(), "Delete Selected Playlist Items");
            audiumEngine->getUndoManager()->beginNewTransaction();
            
            return true;
        }
        
        return false;
    }
    
    
    
private:
    std::shared_ptr<PlayListContainer> playListContainer;
    std::shared_ptr<PlayListItem> playListItem;
    
};
