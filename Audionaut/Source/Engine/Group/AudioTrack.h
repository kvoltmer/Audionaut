//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include "Engine/Streamable.h"
#include "Engine/TimeContext.h"
#include "Engine/Core/DspClipData.h"
#include "Engine/Core/AudioClipContainer.h"
#include "Engine/Selection/Selectable.h"
#include "Engine/Selection/SelectionManager.h"
#include "Engine/Selection/SelectableObjectContainer.h"
#include "Engine/Undo/UndoableContainerAction.h"
#include "Engine/PlayList/PlayListContainer.h"

namespace audium {

class AudioResourceContainer;
class AudioResource;
class PlayListItem;
class PlayListScheduler;
class TransportSourceContainer;
class ResourceGroup;
class AudioChannel;
class PositionableBase;
class AudioTrackContainer;
class AudioChannelData;

typedef SelectableObjectContainer<ResourceGroup> tResourceGroupContainer;
typedef SelectableObjectContainer<AudioChannel> tAudioChannelContainer;

/**
 * @class AudioTrack
 * @brief Represents an audio track in the application.
 *
 * The `AudioTrack` class provides functionality for managing audio resources, channels,
 * playlists, and other track-related operations. It supports undoable actions, selection,
 * and serialization.
 */
class AudioTrack : public Streamable, public Selectable
{
    
public:
    /**
     * @brief Constructs an `AudioTrack` instance.
     * @param owner_ Reference to the owning `AudioTrackContainer`.
     * @param audioResourceContainer_ Reference to the container holding audio resources.
     * @param transportSourceContainer_ Shared pointer to the transport source container.
     * @param selectionManager_ Shared pointer to the selection manager.
     * @param resourceGroups_ Shared pointer to the resource group container.
     * @param channels_ Shared pointer to the audio channel container.
     * @param nameString_ The name of the audio track.
     */
    AudioTrack(AudioTrackContainer &owner_,
               AudioResourceContainer &audioResourceContainer_,
               std::shared_ptr<TransportSourceContainer> transportSourceContainer_,
               std::shared_ptr<SelectionManager> selectionManager_,
               std::shared_ptr<tResourceGroupContainer> resourceGroups_,
               std::shared_ptr<tAudioChannelContainer> channels_,
               juce::String nameString_) :
        Selectable(selectionManager_),
        owner(owner_),
        audioResourceContainer(audioResourceContainer_),
        transportSourceContainer(transportSourceContainer_),
        selectionManager(selectionManager_),
        resourceGroupContainer(resourceGroups_),
        audioChannelContainer(channels_),
        name(nameString_.toStdString())
    {
        playListContainer = std::shared_ptr<PlayListContainer> (new PlayListContainer(*this,
                                                                                      owner.getTempoProvider(),
                                                                                      transportSourceContainer,
                                                                                      selectionManager));
    }
    
    /**
     * @brief Destructor for `AudioTrack`.
     */
    virtual ~AudioTrack() override;
    
    /**
     * @brief Cleans up resources associated with the audio track.
     */
    void cleanup() override;
    
    /**
     * @brief Gets the name of the audio track.
     * @return The name of the audio track.
     */
    const juce::String getAudioTrackName() const { return name; }
    
    /**
     * @brief Sets the name of the audio track.
     * @param newName The new name for the audio track.
     */
    void setAudioTrackName(const juce::String newName);
    
    /**
     * @brief Sets the color of the audio track.
     * @param colour The new color.
     */
    void setColour(juce::Colour colour);
    
    /**
     * @brief Gets the color of the audio track.
     * @return The current color.
     */

    juce::Colour getColour() const { return groupColour; }
    
    /**
     * @brief Gets the index of the audio track within its container.
     * @return The index of the track.
     */
    const int getId() const;
    
    /**
     * @brief Gets the channel offset of the audio track.
     * @return The channel offset.
     */
    const int getChannelOffset() const;
    
    // pointer and references to other classes:
    AudioResourceContainer &getAudioResourceContainer() const { return audioResourceContainer; }
    AudioTrackContainer &getAudioTrackContainer() const { return owner; }
    std::shared_ptr<PlayListContainer> getPlayListContainer() const { return playListContainer; }
    std::shared_ptr<TransportSourceContainer> getTransportSourceContainer() const { return transportSourceContainer; }
    std::shared_ptr<SelectionManager> getSelectionManager() const noexcept { return selectionManager; }
    
    /**
     * @brief Gets all audio resources associated with the track.
     * @return A vector of shared pointers to `AudioResource` instances.
     */
    std::vector<std::shared_ptr<AudioResource>> getAudioResources() const;
    
    
    // Streamable overrides:
    bool writeToStream (juce::OutputStream& outputStream) override;
    bool readFromStream (juce::InputStream& inputStream, bool rebuild) override;
    bool writeToJson (json& output) override;
    bool readFromJson (json& input, bool rebuild) override;
    int getSizeInUnits() override;
    bool writeChannelToJson (json& output, AudioChannel* audioChannel);
    void mergeChannelFromJson(json& input);
    
    // Selectable override:
    void setSelected(bool bSelected, bool selectChildren) override;
    
    void setGain(float gain, int channelNumber);
    float getGain(int channelNumber) const;
    
    void setPan(float pan, int channelNumber);
    float getPan(int channelNumber) const;
    
    void setMute(bool bMute, int channelNumber);
    bool getMute(int channelNumber) const;
    
    void setSolo(bool bSolo, int channelNumber);
    bool getSolo(int channelNumber) const;
    
    void setChannelData(const int channelNumber, const AudioChannelData data);
    const AudioChannelData getChannelData(const int channelNumber) const;
    
    void setRecordEnabled(const int channelNumber, bool bEnabled, std::shared_ptr<juce::AudioThumbnail> thumbnail);
    
    // undo for continious parameters:
    void onDragStart();
    void onDragEnd();
    
    // drag & drop:
    void dropSelectedAudioRegions(int insertIndex);
    void dropSelectedAudioRegions(double pos, TimeContextType context);
    void dropPlayListItem(std::shared_ptr<PlayListItem> item,
                          double pos,
                          TimeContextType context,
                          bool newPlayListItem = false);
    
    // resource groups:
    std::shared_ptr<ResourceGroup> createNewResourceGroup();
    std::shared_ptr<ResourceGroup> createNewResourceGroup(const std::shared_ptr<ResourceGroup> otherResourceGroup);
    std::shared_ptr<ResourceGroup> findExistingResourceGroup(json jsonResourceGroup);
    std::shared_ptr<ResourceGroup> getDefaultResourceGroup() const;
    std::vector<std::shared_ptr<ResourceGroup>> getResourceGroups() const { return resourceGroupContainer->getObjects(); }
    
    // channel height:
    int getTotalHeight() const;
    void setChannelHeight(int height);
    
    std::list<std::shared_ptr<PositionableBase>> getPositionableItems() const;
    
    // objects:
    bool deleteSelectedObject(std::shared_ptr<Selectable> object, bool &rebuild);
    
    // channels:
    int getNumAudioTrackChannels() const;
    void ensureNumChannels(int channelsNeeded);
    std::shared_ptr<AudioChannel> addChannel();
    std::shared_ptr<AudioChannel> getChannel(int channelNumber) const;
    bool deleteChannel(AudioChannel* channel);
    void deleteEmptyResourceGroups();
    void deleteUnusedResourceGroups();
    
    bool addAudioFiles(const juce::StringArray& filenames,
                       double positionClocks,
                       bool arrangementMode,
                       std::function<void (std::string)> callback);
    
    std::vector<std::shared_ptr<AudioResource>> addAudioFile (std::shared_ptr<ResourceGroup> resourceGroup,
                                                              const juce::File filename,
                                                              int &destChannel);
    
    void createDefaultPlayListItem(std::shared_ptr<AudioResource> audioResource,
                                   std::shared_ptr<ResourceGroup> resourceGroup,
                                   double position,
                                   TimeContextType context);
    
    double getTotalLength(TimeContextType context) const;
    
    std::vector<DspClipData> getDspClipVector(bool arrangementMode) const;
    
    std::shared_ptr<AudioRegion> getRegion(int rowNumber) const;
    const std::vector<std::shared_ptr<AudioRegion>> getRegions() const;
    int getAudioRegionId(std::shared_ptr<const AudioRegion> searchRegion) const;
    int lastRegionSelected = -1; ///< Index of the last region selected.
    
private:
    AudioTrackContainer &owner; ///< Reference to the owning container.
    AudioResourceContainer &audioResourceContainer; ///< Reference to the audio resource container.
    std::shared_ptr<PlayListContainer> playListContainer; ///< Shared pointer to the playlist container.
    std::shared_ptr<TransportSourceContainer> transportSourceContainer; ///< Shared pointer to the transport source container.
    std::shared_ptr<SelectionManager> selectionManager; ///< Shared pointer to the selection manager.

public:
    std::shared_ptr<tResourceGroupContainer> resourceGroupContainer; ///< Shared pointer to the resource group container.
    std::shared_ptr<tAudioChannelContainer> audioChannelContainer; ///< Shared pointer to the audio channel container.

private:
    std::string name; ///< Name of the audio track.
    juce::Colour groupColour = juce::Colours::pink; ///< Color of the audio track.
    std::unique_ptr<UndoableContainerAction> undoableContainerAction; ///< Undoable action container.
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioTrack)
    
};

} // namespace audium

