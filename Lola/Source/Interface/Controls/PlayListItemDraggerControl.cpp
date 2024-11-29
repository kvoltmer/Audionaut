/*
  ==============================================================================

    PlayListItemDraggerControl.cpp
    Created: 29 Nov 2024 12:00:23pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "PlayListItemDraggerControl.h"

void PlayListItemDraggerControl::commitData(const juce::Range<double> newData, audium::TimeContextType context)
{
    // undo
    // TODO: move to base class
    if (undoableContainerAction == nullptr)
    {
        undoableContainerAction = std::make_unique<audium::UndoableContainerAction>(*audiumEngine->getAudioTrackContainer(), false);
    }
    
    commitPositionData(*playListItem.get(), newData, context);
}

bool PlayListItemDraggerControl::validateData()
{
    bool result = playListItem->validateData();
    
    // sort by position
    playListContainer->sortByPosition();
    
    // TODO: move to base class
    if (undoableContainerAction != nullptr)
    {
        // Undo: store new state
        undoableContainerAction->storeNewState();
        audiumEngine->getUndoManager()->perform(undoableContainerAction.release(), "Modify Item");
        audiumEngine->getUndoManager()->beginNewTransaction();
    }
    
    return result;
}

