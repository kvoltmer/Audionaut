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
    PlayListContainer(std::shared_ptr<AudioRegionContainer> audioRegionContainer) :
        audioRegionContainer(audioRegionContainer)
    {
    }
    
    void createPlayListItem(std::shared_ptr<AudioRegion> audioRegion);
    void createPlayListItem(int regionIndex, int indexOfItemToPlaceBefore);
    void deletePlayListItem(int atIndex, bool sendNotification = true);
    void deleteAssociatedItems(std::shared_ptr<AudioRegion> audioRegion);
    
    int getNumItems() const
    {
        return static_cast<int>(playListItems.size());
    }
    
    bool writeToStream (juce::OutputStream& outputStream);
    bool readFromStream (juce::InputStream& inputStream);

    std::shared_ptr<PlayListItem> getPlayListItem(int index) const;
    
    std::vector<std::shared_ptr<PlayListItem>> playListItems;
    
private:
    
    std::shared_ptr<AudioRegionContainer> audioRegionContainer;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListContainer)
};
