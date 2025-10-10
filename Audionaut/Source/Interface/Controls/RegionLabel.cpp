//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "RegionLabel.h"

void RegionLabel::mouseDown (const juce::MouseEvent& e)
{
    auto region = getRegion(rowNumber);
    bool isSelected = region != nullptr && region->isSelected();
    
    if (!e.mods.isAnyModifierKeyDown() && !isSelected) {
        audioTrackContainer->getSelectionManager()->deselectAll();
    }
    
    /// pass on mouse events. unless row is not selected
    getParentComponent()->mouseDown(e);
    
    // update
    audioTrackContainer->sendActionMessage(audium::updateSelection);
}
