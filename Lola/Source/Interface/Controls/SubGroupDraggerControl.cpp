/*
  ==============================================================================

    SubGroupDraggerControl.cpp
    Created: 29 Nov 2024 12:00:55pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "SubGroupDraggerControl.h"

void SubGroupDraggerControl::commitData(const juce::Range<double> newData, audium::TimeContextType context)
{
    // undo
    // TODO: move to base class
    if (undoableContainerAction == nullptr)
    {
        undoableContainerAction = std::make_unique<audium::UndoableContainerAction>(*audiumEngine->getAudioTrackContainer(), false);
    }
    
    jassert(audioSubGroup->getAudioResources().size() > 0);
    
    commitPositionData(*audioSubGroup.get(), newData, context);
}

bool SubGroupDraggerControl::validateData()
{
    bool result = audioSubGroup->getAudioClip()->validateData();
    
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

