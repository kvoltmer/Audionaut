
#include "PlayListComponent.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Interface/Controls/RegionEditor.h"

void PlayListComponent::itemDropped (const SourceDetails &dragSourceDetails)
{
    auto insertIndex = static_cast<int>(audiumEngine->getPlayListContainer()->playListItems.size());
    
    if ( PlayListTableListBoxItem* item = dynamic_cast<PlayListTableListBoxItem*>(dragSourceDetails.sourceComponent.get()))
    {
        MoveItemBefore(audiumEngine->getPlayListContainer()->playListItems,
                       item->rowNumber,
                       insertIndex);
    }
    else if ( RegionEditor* item = dynamic_cast<RegionEditor*>(dragSourceDetails.sourceComponent.get()))
    {
        audiumEngine->getPlayListContainer()->createPlayListItem(item->getRowNumber(), insertIndex);
    }
    
    triggerAsyncUpdate();
    
    //hideInsertLines();
}
