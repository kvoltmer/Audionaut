

#include "ChannelsComponent.h"

bool ChannelsComponent::isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails &dragSourceDetails)
{
    if (auto item = dynamic_cast<ChannelComponent*>(dragSourceDetails.sourceComponent.get())) {
        //if (item->getPlayListModel() == playListModel)
        {
            // return true if source details match this model
            return true;
        }
    }
    return false;
}

void ChannelsComponent::itemDropped (const SourceDetails &dragSourceDetails)
{
//    auto action = std::make_unique<audium::UndoableContainerAction>(audioTrack->getAudioTrackContainer());
//    
//    auto insertIndex = static_cast<int>(audioTrack->getPlayListContainer()->playListItems.size());
//    
//    if ( PlayListTableListBoxItem* item = dynamic_cast<PlayListTableListBoxItem*>(dragSourceDetails.sourceComponent.get()))
//    {
//        audioTrack->getPlayListContainer()->movePlayListItemBefore(item->rowNumber,
//                                                                   insertIndex);
//    }
//    else if ( RegionLabel* item = dynamic_cast<RegionLabel*>(dragSourceDetails.sourceComponent.get()))
//    {
//        bool success = false;
//        auto pos = 0.0;
//        if (audioTrack->getPlayListContainer()->playListItems.objects.size() > 0) {
//            auto last = audioTrack->getPlayListContainer()->playListItems.objects.back();
//            pos = last->getAbsolutePositionRange(audium::clocks).getEnd();
//        }
//        
//        // drop all selected regions
//        auto selectedRegions = audioTrack->getAudioRegionContainer()->getSelectedRegions();
//        for (auto region : selectedRegions)
//        {
//            if (audioTrack->getPlayListContainer()->createPlayListItemAtPositionUI(region, pos, audium::clocks) != nullptr)
//                success = true;
//            
//            pos += region->getRegionData(audium::clocks).getLength();
//        }
//        jassert(success);
//        
//    }
//    
//    action->storeNewState();
//    auto undoManager = audioTrack->getAudioTrackContainer().getUndoManager();
//    undoManager->perform(action.release(), "Playlist modified");
//    undoManager->beginNewTransaction();
//    
//    
//    triggerAsyncUpdate();
    
    
    itemDrag = false;
    repaint();
}


void ChannelsComponent::paint (juce::Graphics& g)
{
    if (itemDrag) {
        auto height = audioChannelsListBox->getHeaderComponent()->getHeight();
        for (auto r = 0; r < audioChannelsListBoxModel->getNumRows(); r++) {
            height += audioChannelsListBoxModel->getRowHeight(r);
        }
        g.setColour(findColour(audium::secondaryBackgroundColourId).brighter().withAlpha(0.5f));
        g.fillRect(0, height, getWidth(), getHeight() - height);
    }
}
