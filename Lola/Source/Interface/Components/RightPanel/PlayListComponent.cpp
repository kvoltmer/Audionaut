
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
    
    if ( RegionLabel* item = dynamic_cast<RegionLabel*>(dragSourceDetails.sourceComponent.get()))
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


void PlayListComponent::updateInsertLines(const juce::DragAndDropTarget::SourceDetails &dragSourceDetails)
{
    auto height = playListTableListBox->getHeaderHeight();
    height += playListTableListBox->getRowHeight() * playListTableListBoxModel->getNumRows();
    
    auto after = dragSourceDetails.localPosition.y > height;
    
    if (auto item = dynamic_cast<RegionLabel*>(dragSourceDetails.sourceComponent.get()) &&
        after) {
        itemDrag = true;
    }

    repaint();
}

void PlayListComponent::paint(juce::Graphics &g)
{
    if (itemDrag) {
        g.setColour(audioTrack->getColour());
        auto height = playListTableListBox->getHeaderHeight();
        height += playListTableListBox->getRowHeight() * playListTableListBoxModel->getNumRows();
        g.fillRect(0, height, getWidth(), 3);
    }

}
