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
        // Select the region in the engine here, as a click on the row would.
        // The row selects on mouse-down only when it is not highlighted yet;
        // a row highlighted because a clip using the region is selected
        // (display only, see RegionComponent::updateSelection) waits for the
        // mouse-up - which a drag out of the list gets only after the drop.
        if (region != nullptr) {
            juce::SparseSet<int> row;
            row.addRange ({ rowNumber, rowNumber + 1 });
            audioTrackContainer->getAudioRegionAdapter().setSelectedRows(row);
        }
        else {
            audioTrackContainer->getSelectionManager()->deselectAll();
        }
    }
    
    /// pass on mouse events. unless row is not selected
    getParentComponent()->mouseDown(e);
    
    // update
    audioTrackContainer->sendActionMessage(audium::updateSelection);
}
