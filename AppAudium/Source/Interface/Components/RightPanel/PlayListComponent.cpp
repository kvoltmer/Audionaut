
#include "PlayListComponent.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Interface/Controls/RegionLabel.h"
#include "Engine/Group/AudioGroupContainer.h"

void PlayListComponent::itemDropped (const SourceDetails &dragSourceDetails)
{
    auto action = std::make_unique<audium::UndoableContainerAction>(audioGroup->getAudioGroupContainer());
    
    auto insertIndex = static_cast<int>(audioGroup->getPlayListContainer()->playListItems.size());
    
    if ( PlayListTableListBoxItem* item = dynamic_cast<PlayListTableListBoxItem*>(dragSourceDetails.sourceComponent.get()))
    {
        audioGroup->getPlayListContainer()->movePlayListItemBefore(item->rowNumber,
                                                                   insertIndex);
    }
    else if ( RegionLabel* item = dynamic_cast<RegionLabel*>(dragSourceDetails.sourceComponent.get()))
    {
        if (audioGroup->getPlayListContainer()->createPlayListItemUI(item->getRowNumber(), insertIndex) == nullptr)
        {
            return;
        }
    }
    
    action->storeNewState();
    auto undoManager = audioGroup->getAudioGroupContainer().getUndoManager();
    undoManager->perform(action.release(), "Playlist modified");
    undoManager->beginNewTransaction();
    
    
    triggerAsyncUpdate();
    
    //hideInsertLines();
}
