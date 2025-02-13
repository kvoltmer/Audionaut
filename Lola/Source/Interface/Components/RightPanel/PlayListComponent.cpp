
#include "PlayListComponent.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Interface/Controls/RegionLabel.h"
#include "Interface/Controls/TableRegionLabel.h"
#include "Engine/Group/AudioTrackContainer.h"


bool PlayListComponent::isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails &dragSourceDetails)
{
    if (dynamic_cast<RegionLabel*>(dragSourceDetails.sourceComponent.get()) != nullptr)
        return true;

    if (dynamic_cast<TableRegionLabel*>(dragSourceDetails.sourceComponent.get()) != nullptr)
        return true;

    
    return false;
}

void PlayListComponent::itemDropped (const SourceDetails &dragSourceDetails)
{
    auto action = std::make_unique<audium::UndoableContainerAction>(audioTrack->getAudioTrackContainer());
    
    if (dynamic_cast<RegionLabel*>(dragSourceDetails.sourceComponent.get()) != nullptr ||
        dynamic_cast<TableRegionLabel*>(dragSourceDetails.sourceComponent.get()) != nullptr) {
        
        bool success = false;
        auto pos = 0.0;
        if (audioTrack->getPlayListContainer()->playListItems.objects.size() > 0) {
            auto last = audioTrack->getPlayListContainer()->playListItems.objects.back();
            pos = last->getAbsolutePositionRange(audium::clocks).getEnd();
        }
        
        audioTrack->dropSelectedAudioRegions(pos, audium::clocks);
        success = true;
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
    
    if ((dynamic_cast<RegionLabel*>(dragSourceDetails.sourceComponent.get()) != nullptr ||
         dynamic_cast<TableRegionLabel*>(dragSourceDetails.sourceComponent.get()) != nullptr) &&
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
