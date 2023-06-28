/*
  ==============================================================================

    PlayListContainer.h
    Created: 28 Jun 2023 11:50:56am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <vector>
#include <memory>
#include <JuceHeader.h>

class PlayListItem;
class AudioRegion;

class PlayListContainer {
    
public:
    PlayListContainer() = default;
    
    void createPlayListItem(std::shared_ptr<AudioRegion> audioRegion);
    
    int getNumItems() const { return static_cast<int>(playListItems.size()); }
    std::shared_ptr<PlayListItem> getPlayListItem(int index) const;
    
private:
    
    std::vector<std::shared_ptr<PlayListItem>> playListItems;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListContainer)
};
