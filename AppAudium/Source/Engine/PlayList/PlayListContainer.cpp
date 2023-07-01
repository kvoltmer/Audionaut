/*
  ==============================================================================

    PlayListContainer.cpp
    Created: 28 Jun 2023 11:50:56am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/AudioRegion.h"
#include "Engine/AudioRegionContainer.h"

void PlayListContainer::createPlayListItem(std::shared_ptr<AudioRegion> audioRegion)
{
    auto playListItem = std::shared_ptr<PlayListItem>(new PlayListItem(audioRegion));
    playListItems.push_back(playListItem);
    
    //sendActionMessage(regionCreatedAction);
}

void PlayListContainer::createPlayListItem(int regionIndex, int indexOfItemToPlaceBefore)
{
    jassert( indexOfItemToPlaceBefore >= 0);
    jassert( indexOfItemToPlaceBefore <= playListItems.size());
    
    auto region = audioRegionContainer->getRegion(regionIndex);
    jassert(region != nullptr);
    
    auto playListItem = std::shared_ptr<PlayListItem>(new PlayListItem(region));
    playListItems.insert(playListItems.begin() + indexOfItemToPlaceBefore, playListItem);
    
    //sendActionMessage(regionCreatedAction);
}



std::shared_ptr<PlayListItem> PlayListContainer::getPlayListItem(int index) const
{
    if (index >= 0 && index < playListItems.size())
    {
        return playListItems[index];
    }
    return nullptr;
}
