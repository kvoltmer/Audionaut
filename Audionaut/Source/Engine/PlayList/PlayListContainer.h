//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

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
class VoiceSourceContainer;
class AudioChannel;

/**
 * \class PlayListContainer
 * \brief Manages a collection of playlist items for an audio track.
 *
 * This class provides functionality to create, delete, move, and manage playlist items
 * associated with an audio track. It also supports serialization to and from JSON.
 */
class PlayListContainer : public audium::Streamable
{
    
public:
    PlayListContainer(const AudioTrack &audioTrack_,
                      std::shared_ptr<TempoProvider> tempoProvider_,
                      std::shared_ptr<VoiceSourceContainer> voiceSourceContainer_,
                      std::shared_ptr<SelectionManager> selectionManager_) :
        audioTrack(audioTrack_),
        tempoProvider(tempoProvider_),
        voiceSourceContainer(voiceSourceContainer_),
        selectionManager(selectionManager_)
    {
    }
    
    ~PlayListContainer() override;
    
    // called from UI
    std::shared_ptr<PlayListItem> createPlayListItemAtPositionUI(std::shared_ptr<AudioRegion> audioRegion,
                                                                 double position,
                                                                 audium::TimeContextType context);
    std::shared_ptr<PlayListItem> createPlayListItemUI(std::shared_ptr<AudioRegion> region,
                                                       int indexOfItemToPlaceBefore);
    
    std::shared_ptr<PlayListItem> clonePlayListItem(std::shared_ptr<PlayListItem> item);
    
    void movePlayListItemBefore(int currentIndex, int indexOfItemToPlaceBefore);
    
    void deletePlayListItem(int atIndex);
    bool deletePlayListItem(PlayListItem* playListItem, bool deleteRegion);
    
    bool deleteAssociatedItems(const AudioRegion* audioRegion);
    bool exitsInPlayList(const AudioRegion* audioRegion);
    
    const std::vector<std::shared_ptr<PlayListItem>> &getPlayListItems() const;
    int getNumItems(std::shared_ptr<AudioTrack> track = nullptr) const;
    
    bool writeToJson (json& output) override;
    bool readFromJson (json& input, bool rebuild) override;
    // writes the playlist items with the given channel's clip gain as "channel_clip_gain"
    bool writeChannelToJson(json& output, AudioChannel* audioChannel);
    void mergeFromJson(json& input, int destinationChannel = -1);
    bool playListItemExists(std::shared_ptr<PlayListItem> item) const;
    std::shared_ptr<PlayListItem> findExistingItem(std::shared_ptr<PlayListItem> item) const;
    std::shared_ptr<PlayListItem> createPlayListItemFromJson (json& jsonItem);
    
    int getSizeInUnits() override;
    
    
    
    std::shared_ptr<PlayListItem> getPlayListItem(int index) const;
    int getPlayListItemIndex(PlayListItem* item) const;
    
    PlayListItem* itemAtAbsolutePosition(double position, audium::TimeContextType context) const;
    PlayListItem* itemIntersectingRange(juce::Range<double> range, audium::TimeContextType context) const;
    PlayListItem* itemContainingRange(juce::Range<double> range, audium::TimeContextType context) const;
    
    const std::vector<PlayListItem*> itemsAtAbsoluteRange(juce::Range<double> range, audium::TimeContextType context) const;
    
    double getAbsolueStartTimeByOrder(const PlayListItem* playListItem, audium::TimeContextType context) const;
    
    // sets the absolute position based on the order
    void forcePositionByOrder();
    
    void sortByPosition();
    bool sortedByPosition() const;
    
    double findNextFreePosition(double position, audium::TimeContextType context) const;
    
    // move all playlist items at start index
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
    std::shared_ptr<VoiceSourceContainer> voiceSourceContainer;
    std::shared_ptr<SelectionManager> selectionManager;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListContainer)
};

} // namespace audium
