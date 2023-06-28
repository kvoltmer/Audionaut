/*
  ==============================================================================

    PlayListItem.cpp
    Created: 28 Jun 2023 11:51:10am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "PlayListItem.h"

PlayListItem::PlayListItem(std::shared_ptr<AudioRegion> audioRegion) :
    audioRegion(audioRegion)
{
}
