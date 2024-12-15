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
    
    
    // must call deselect since items of other playlists might be selected
    //auto track = playListModel->getAudioTrack();
    if (audioRegionContainer != nullptr &&
        !e.mods.isAnyModifierKeyDown())
    {
        audioTrackContainer->getSelectionManager()->deselectAll();
    }
    
    /// pass on mouse events. unless row is not selected
    getParentComponent()->mouseDown(e);
    
//    // select this item
//    auto container = playListModel->getAudioTrack()->getPlayListContainer();
//    if (auto item = container->getPlayListItem(rowNumber))
//        item->setSelected(true);
//
//    // pass on the event to the model
//    getParentComponent()->mouseDown(e);
//
//    // update
    audioTrackContainer->sendActionMessage(updateSelection);
}
