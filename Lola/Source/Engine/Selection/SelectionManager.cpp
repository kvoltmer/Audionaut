/*
  ==============================================================================

    SelectionManager.cpp
    Created: 6 Oct 2024 11:59:59am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "SelectionManager.h"
#include "Selectable.h"
#include "Engine/Group/AudioGroupContainer.h"
#include "Engine/Group/AudioGroup.h"
#include "Engine/Region/AudioRegion.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/Channel/AudioChannel.h"

namespace audium {

void SelectionManager::deselectAll() {
    auto objects = selectedObjects;
    for (auto object : objects)
        object->setSelected(false);
    
    jassert(selectedObjects.size() == 0);
}





void SelectionManager::copySelectedToClipboard() {
    
    // TODO: introduce an object scope
    
    json jout;
    
    for (auto object : selectedObjects) {
        json j;
        if (AudioGroup* group = dynamic_cast<AudioGroup*>(object.get())) {
            group->writeToJson(j);
            jout["groups"] += j;
        }
        if (AudioSubGroup* subgroup = dynamic_cast<AudioSubGroup*>(object.get())) {
            subgroup->writeToJson(j);
            jout["sub_group"] += j;
        }
        else if (AudioChannel* channel = dynamic_cast<AudioChannel*>(object.get())) {
            jout["channels"] += channel->data;
        }
        else if (AudioRegion* region = dynamic_cast<AudioRegion*>(object.get())) {
            region->writeToJson(j);
            jout["regions"] += j;
        }
        else if (PlayListItem* playListItem = dynamic_cast<PlayListItem*>(object.get())) {
            playListItem->writeToJson(j);
            jout["play_list_items"] += j;
        }
        
    }
    
    juce::String jsonText = jout.dump(2);
    
    std::cout << jsonText << std::endl;
    
    juce::SystemClipboard::copyTextToClipboard (jsonText);
}

} // namespace audium
