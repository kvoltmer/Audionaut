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

#include "Engine/Region/AudioRegion.h"
#include "Engine/Streamable.h"

class PlayListItem;
class AudioRegionContainer;
class TransportSourceContainer;


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

class PlayListContainer : public audium::Streamable
{
    
public:
    PlayListContainer(const AudioRegionContainer &audioRegionContainer,
                      std::shared_ptr<TempoProvider> tempoProvider,
                      std::shared_ptr<TransportSourceContainer> transportSourceContainer) :
        audioRegionContainer(audioRegionContainer),
        tempoProvider(tempoProvider),
        transportSourceContainer(transportSourceContainer)
    {
    }
    
    ~PlayListContainer();
    
    // called from UI
    std::shared_ptr<PlayListItem> createPlayListItemAtPositionUI(std::shared_ptr<AudioRegion> audioRegion,
                                                                 juce::Range<double> position,
                                                                 audium::TimeContextType context);
    std::shared_ptr<PlayListItem> createPlayListItemUI(int regionIndex,
                                                       int indexOfItemToPlaceBefore);

    void movePlayListItemBefore(int currentIndex, int indexOfItemToPlaceBefore);
    
    void deletePlayListItem(int atIndex, bool sendNotification = true);
    bool deletePlayListItem(PlayListItem* playListItem);

    bool deleteAssociatedItems(const AudioRegion* audioRegion);
    
    const std::vector<std::shared_ptr<PlayListItem>> getPlayListItems() const;
    int getNumItems(std::shared_ptr<AudioGroup> group = nullptr) const;
    
    bool writeToStream (juce::OutputStream& outputStream) override;
    bool readFromStream (juce::InputStream& inputStream, bool rebuild) override;
    bool writeToJson (json& output) override;
    bool readFromJson (json& input, bool rebuild) override;
    int getSizeInUnits() override;
    
    void cleanup() { playListItems.clear(); }

    std::shared_ptr<PlayListItem> getPlayListItem(int index) const;
    int getPlayListItemIndex(const PlayListItem* item) const;
    
    AudioRegionData::tRange getPlayListDataAtIndex(int index) const;
    
    const PlayListItem* itemAtAbsolutePosition(double position, audium::TimeContextType context) const;
    const PlayListItem* itemAtAbsoluteRange(juce::Range<double> range, audium::TimeContextType context) const;
    
    const std::vector<PlayListItem*> itemsAtAbsoluteRange(juce::Range<double> range, audium::TimeContextType context) const;
    
    double getAbsolueStartTimeByOrder(const PlayListItem* playListItem, audium::TimeContextType context) const;
    
    // sets the absolute position based on the order
    void forcePositionByOrder();
    
    // move the absolute position of all playlist items by an amount
    void movePlayListItemsPosition(int startIndex, double amount, audium::TimeContextType context);
    
    std::vector<std::shared_ptr<PlayListItem>> playListItems;
            
    // selection:
    void selectAllItems(bool bSelected);
    juce::SparseSet<int> getSelectedRows() const;
    void setSelectedRows(juce::SparseSet<int>& selectedRows);
    void selectPlayListItemWithRegion(std::shared_ptr<AudioRegion> region);
    
    double getTotalLength(audium::TimeContextType context) const;
    
    const AudioRegionContainer &getAudioRegionContainer() const { return audioRegionContainer; }
    std::shared_ptr<TempoProvider> getTempoProvider() const noexcept { return tempoProvider; }

private:
    
    // called internally
    std::shared_ptr<PlayListItem> createPlayListItem(std::shared_ptr<AudioRegion> audioRegion, int insertIndex);
    std::shared_ptr<PlayListItem> createPlayListItem(int regionIndex, int indexOfItemToPlaceBefore);
    
    juce::CriticalSection readLock;
    
    const AudioRegionContainer &audioRegionContainer;
    std::shared_ptr<TempoProvider> tempoProvider;
    std::shared_ptr<TransportSourceContainer> transportSourceContainer;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListContainer)
};
