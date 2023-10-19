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

#include "Engine/AudioRegion.h"

class PlayListItem;
class AudioRegionContainer;


template<typename C>
void MoveItemBefore(C& container, size_t currentIndex, size_t indexOfItemToPlaceBefore)
{
    if( currentIndex == indexOfItemToPlaceBefore ) return;
    
    jassert( juce::isPositiveAndBelow((int)currentIndex, (int)container.size() ));
    jassert( juce::isPositiveAndBelow((int)indexOfItemToPlaceBefore, (int)container.size() + 1 ));
    
    if (currentIndex < indexOfItemToPlaceBefore)
    {
        std::rotate(container.begin() + currentIndex,
                    container.begin() + currentIndex + 1,
                    container.begin() + indexOfItemToPlaceBefore);
    }
    else
    {
        std::rotate(container.begin() + indexOfItemToPlaceBefore,
                    container.begin() + currentIndex,
                    container.begin() + currentIndex + 1);
    }
}

class PlayListContainer : public juce::ActionBroadcaster {
    
public:
    PlayListContainer(const AudioRegionContainer &audioRegionContainer) :
        audioRegionContainer(audioRegionContainer)
    {
    }
    ~PlayListContainer();
    
    
    void createPlayListItem(std::shared_ptr<AudioRegion> audioRegion);
    void createPlayListItem(int regionIndex, int indexOfItemToPlaceBefore);
    void deletePlayListItem(int atIndex, bool sendNotification = true);
    void deleteAssociatedItems(std::shared_ptr<AudioRegion> audioRegion);
    
    const std::vector<std::shared_ptr<PlayListItem>> getPlayListItems() const;
    int getNumItems(std::shared_ptr<AudioGroup> group = nullptr) const;
    
    bool writeToStream (juce::OutputStream& outputStream);
    bool readFromStream (juce::InputStream& inputStream);
    
    void cleanup() { playListItems.clear(); }

    std::shared_ptr<PlayListItem> getPlayListItem(int index) const;
    int getPlayListItemIndex(const PlayListItem* item) const;
    
    AudioRegion::RegionData getPlayListDataAtIndex(int index) const;
    
    const PlayListItem* itemAtAbsolutePosition(double position) const;
    const PlayListItem* itemAtAbsoluteRange(juce::Range<double> range) const;
    
    double getAbsolueStartTime(const PlayListItem* playListItem) const;
    
    std::vector<std::shared_ptr<PlayListItem>> playListItems;
    
    const PlayListItem* currentPlayListItem = nullptr;
    
    void selectPlayListItem(std::shared_ptr<PlayListItem> item, bool bSelected);
    
    void deselectAll();
    
private:
    
    
    juce::CriticalSection readLock;
    
    const AudioRegionContainer &audioRegionContainer;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListContainer)
};
