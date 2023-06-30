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
    jassert( juce::isPositiveAndBelow((int)indexOfItemToPlaceBefore, (int)container.size() ));
    
    if (currentIndex < indexOfItemToPlaceBefore)
    {
        std::rotate(container.begin() + currentIndex,
                    container.begin() + currentIndex + 1,
                    container.begin() + indexOfItemToPlaceBefore);
    }
    else
    {
        std::rotate(container.begin() + indexOfItemToPlaceBefore, //thanks to @Kyran for this fix
                    container.begin() + currentIndex,
                    container.begin() + currentIndex + 1);
    }
}

template<typename C>
void MoveItemAfter(C& container, size_t currentIndex, size_t indexOfItemToPlaceAfter)
{
    if( currentIndex == indexOfItemToPlaceAfter ) return;
    
    jassert( juce::isPositiveAndBelow((int)currentIndex, (int)container.size() ));
    jassert( juce::isPositiveAndBelow((int)indexOfItemToPlaceAfter, (int)container.size() ));
    
    if (currentIndex < indexOfItemToPlaceAfter)
    {
        std::rotate(container.begin() + currentIndex,
                    container.begin() + currentIndex + 1,
                    container.begin() + indexOfItemToPlaceAfter + 1);
    }
    else
    {
        std::rotate(container.begin() + indexOfItemToPlaceAfter + 1, //thanks to @Kyran for this fix
                    container.begin() + currentIndex,
                    container.begin() + currentIndex + 1);
    }
}

class PlayListContainer {
    
public:
    PlayListContainer(std::shared_ptr<AudioRegionContainer> audioRegionContainer) :
        audioRegionContainer(audioRegionContainer)
    {
    }
    
    void createPlayListItem(std::shared_ptr<AudioRegion> audioRegion);
    void createPlayListItem(int regionNumber, int insertIndex);
    
    int getNumItems() const { return static_cast<int>(playListItems.size()); }
    std::shared_ptr<PlayListItem> getPlayListItem(int index) const;
    
    std::vector<std::shared_ptr<PlayListItem>> playListItems;
    
    std::shared_ptr<AudioRegionContainer> audioRegionContainer;

private:
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListContainer)
};
