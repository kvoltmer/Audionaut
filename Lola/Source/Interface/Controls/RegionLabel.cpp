/*
  ==============================================================================

    RegionLabel.cpp
    Created: 15 Dec 2024 12:42:47pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "RegionLabel.h"

void RegionLabel::mouseDown (const juce::MouseEvent& e)
{
    if (!e.mods.isAnyModifierKeyDown()) {
        audioTrackContainer->getSelectionManager()->deselectAll();
    }
    
    /// pass on mouse events. unless row is not selected
    getParentComponent()->mouseDown(e);
    
    // update
    audioTrackContainer->sendActionMessage(updateSelection);
}
