
#include "PlayListComponent.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Interface/Controls/RegionLabel.h"
#include "Engine/Group/AudioTrackContainer.h"


bool PlayListComponent::isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails &dragSourceDetails)
{
    if (auto regionLabel = dynamic_cast<RegionLabel*>(dragSourceDetails.sourceComponent.get()))
    {
        if (regionLabel->getRegion(regionLabel->getRowNumber()) &&
            regionLabel->getRegion(regionLabel->getRowNumber())->getAudioTrack() == audioTrack)
        {
            // return true if source details match this track
            return true;
        }
    }
    return false;
}

void PlayListComponent::itemDropped (const SourceDetails &dragSourceDetails)
{
    auto action = std::make_unique<audium::UndoableContainerAction>(audioTrack->getAudioTrackContainer());
    
    auto insertIndex = static_cast<int>(audioTrack->getPlayListContainer()->playListItems.size());
    
    if ( PlayListTableListBoxItem* item = dynamic_cast<PlayListTableListBoxItem*>(dragSourceDetails.sourceComponent.get()))
    {
        audioTrack->getPlayListContainer()->movePlayListItemBefore(item->rowNumber,
                                                                   insertIndex);
    }
    else if ( RegionLabel* item = dynamic_cast<RegionLabel*>(dragSourceDetails.sourceComponent.get()))
    {
        bool success = false;
        auto pos = 0.0;
        if (audioTrack->getPlayListContainer()->playListItems.objects.size() > 0) {
            auto last = audioTrack->getPlayListContainer()->playListItems.objects.back();
            pos = last->getAbsolutePositionRange(audium::clocks).getEnd();
        }
        
        // drop all selected regions
        auto selectedRegions = audioTrack->getAudioRegionContainer()->getSelectedRegions();
        for (auto region : selectedRegions)
        {
            if (audioTrack->getPlayListContainer()->createPlayListItemAtPositionUI(region, pos, audium::clocks) != nullptr)
                success = true;
            
            pos += region->getRegionData(audium::clocks).getLength();
        }
        jassert(success);
        
    }
    
    action->storeNewState();
    auto undoManager = audioTrack->getAudioTrackContainer().getUndoManager();
    undoManager->perform(action.release(), "Playlist modified");
    undoManager->beginNewTransaction();
    
    
    triggerAsyncUpdate();
    
    //hideInsertLines();
}
