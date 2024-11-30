/*
  ==============================================================================

    PlayListItemDraggerControl.cpp
    Created: 29 Nov 2024 12:00:23pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "PlayListItemDraggerControl.h"

bool PlayListItemDraggerControl::validateData()
{
    bool result = playListItem->validateData();
    
    // sort by position
    playListContainer->sortByPosition();
    
    
    return result;
}

