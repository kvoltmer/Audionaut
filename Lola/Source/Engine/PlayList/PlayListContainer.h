//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <vector>
#include <memory>
#include <JuceHeader.h>

#include "Engine/Region/AudioRegion.h"
#include "Engine/Streamable.h"
#include "Engine/Selection/SelectableObjectContainer.h"
#include "Engine/PlayList/PlayListItem.h"


namespace audium {

class AudioRegionContainer;
class TransportSourceContainer;

class PlayListContainer : public audium::Streamable
{
    
public:
    PlayListContainer(const AudioTrack &audioTrack_,
                      std::shared_ptr<TempoProvider> tempoProvider_,
                      std::shared_ptr<TransportSourceContainer> transportSourceContainer_,
                      std::shared_ptr<SelectionManager> selectionManager_) :
    audioTrack(audioTrack_),
    tempoProvider(tempoProvider_),
    transportSourceContainer(transportSourceContainer_),
    selectionManager(selectionManager_)
    {
    }
    
    ~PlayListContainer();
    
    // called from UI
    std::shared_ptr<PlayListItem> createPlayListItemAtPositionUI(std::shared_ptr<AudioRegion> audioRegion,
                                                                 double position,
                                                                 audium::TimeContextType context);
    std::shared_ptr<PlayListItem> createPlayListItemUI(std::shared_ptr<AudioRegion> region,
                                                       int indexOfItemToPlaceBefore);
    
    void movePlayListItemBefore(int currentIndex, int indexOfItemToPlaceBefore);
    
    void deletePlayListItem(int atIndex);
    bool deletePlayListItem(PlayListItem* playListItem, bool deleteRegion);
    
    bool deleteAssociatedItems(const AudioRegion* audioRegion);
    bool exitsInPlayList(const AudioRegion* audioRegion);
    
    const std::vector<std::shared_ptr<PlayListItem>> &getPlayListItems() const;
    int getNumItems(std::shared_ptr<AudioTrack> track = nullptr) const;
    
    bool writeToJson (json& output) override;
    bool readFromJson (json& input, bool rebuild) override;
    void mergeFromJson(json& input);
    bool playListItemExists(std::shared_ptr<PlayListItem> item) const;
    std::shared_ptr<PlayListItem> createPlayListItemFromJson (json& jsonItem);
    
    int getSizeInUnits() override;
    
    
    
    std::shared_ptr<PlayListItem> getPlayListItem(int index) const;
    int getPlayListItemIndex(PlayListItem* item) const;
    
    PlayListItem* itemAtAbsolutePosition(double position, audium::TimeContextType context) const;
    PlayListItem* itemAtAbsoluteRange(juce::Range<double> range, audium::TimeContextType context) const;
    
    const std::vector<PlayListItem*> itemsAtAbsoluteRange(juce::Range<double> range, audium::TimeContextType context) const;
    
    double getAbsolueStartTimeByOrder(const PlayListItem* playListItem, audium::TimeContextType context) const;
    
    // sets the absolute position based on the order
    void forcePositionByOrder();
    
    void sortByPosition();
    bool sortedByPosition() const;
    
    double findNextFreePosition(double position, audium::TimeContextType context) const;
    
    // move the absolute position of all playlist items by an amount
    void movePlayListItemsPosition(int startIndex);
    
    // selection:
    void selectPlayListItemWithRegion(std::shared_ptr<AudioRegion> region);
    
    std::vector<std::shared_ptr<PlayListItem>> getSelectedItems(bool global = false) const;
    
    double getTotalLength(audium::TimeContextType context) const;
    
    const AudioTrack &getAudioTrack() const { return audioTrack; }
    
    std::shared_ptr<TempoProvider> getTempoProvider() const noexcept { return tempoProvider; }
    
    // the container
    audium::SelectableObjectContainer<PlayListItem> playListItems;
    
private:
    
    // called internally
    std::shared_ptr<PlayListItem> createPlayListItem(std::shared_ptr<AudioRegion> audioRegion, int insertIndex);
    
    
    const AudioTrack &audioTrack;
    std::shared_ptr<TempoProvider> tempoProvider;
    std::shared_ptr<TransportSourceContainer> transportSourceContainer;
    std::shared_ptr<SelectionManager> selectionManager;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListContainer)
};

} // namespace audium
