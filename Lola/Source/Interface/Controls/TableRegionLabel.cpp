/*
  ==============================================================================

    RegionLabel.cpp
    Created: 15 Dec 2024 12:42:47pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "TableRegionLabel.h"

void TableRegionLabel::mouseDown (const juce::MouseEvent& e)
{
    if (!e.mods.isAnyModifierKeyDown())
    {
        audioTrackContainer->getSelectionManager()->deselectAll();
//        // items of other playlists might be selected
//        // deselect all objects except regions of this track
//        auto objects = audioTrackContainer->getSelectionManager()->getSelectedObjects();
//        for (auto object : objects) {
//            if (auto region = dynamic_cast<AudioRegion*>(object.get())) {
//                if (region->getAudioTrack()->getAudioRegionContainer() == audioRegionContainer)
//                    continue;
//            }
//            object->setSelected(false);
//        }
    }
    
    if (auto region = getRegion(columnId, rowNumber)) {
        region->setSelected(true);
    }
    
    /// pass on mouse events. unless row is not selected
    getParentComponent()->mouseDown(e);
    
    // update
    audioTrackContainer->sendActionMessage(updateSelection);
}
