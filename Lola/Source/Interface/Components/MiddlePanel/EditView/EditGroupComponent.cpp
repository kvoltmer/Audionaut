/*
  ==============================================================================

    EditGroupComponent.cpp
    Created: 18 Apr 2024 2:41:36pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "EditGroupComponent.h"


void EditGroupComponent::filesDropped (const StringArray& filenames, int x, int y)
{
    if ( !filenames.isEmpty())
    {
        // Undo: store old state
        auto action = std::make_unique<audium::UndoableContainerAction>(*audiumEngine->getAudioTrackContainer());
                
        auto position = zoomHandler->xToClocks(x);
        zoomHandler->snapToGrid(position);
        
        std::function<void (std::string)> callback = [](std::string error) {
            juce::NativeMessageBox::showMessageBoxAsync(MessageBoxIconType::WarningIcon,
                                                        "Failed to open File.",
                                                        "Failed to open: " + juce::String(error));
        };
        
        if (audioTrack->addAudioFiles(filenames, position, false, callback))
        {
            action->storeNewState();
            audiumEngine->getUndoManager()->perform(action.release(), "File(s) dropped");
            audiumEngine->getUndoManager()->beginNewTransaction();
        }
    }
    
    // call base class!
    AudioTrackBaseComponent::filesDropped(filenames, x, y);
}
