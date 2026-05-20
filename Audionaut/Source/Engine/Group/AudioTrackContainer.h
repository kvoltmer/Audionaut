//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include "Engine/Streamable.h"
#include "Engine/TimeContext.h"
#include "Engine/Region/AudioRegionData.h"
#include "Engine/Group/AudioRegionAdapter.h"
#include "Engine/Selection/SelectionManager.h"

namespace audium {

class AudioTrack;
class AudioResourceContainer;
class AudioRegionContainer;
class AudiumEngine;
class TempoProvider;
class AudioRegion;
class TransportSourceContainer;
class AudioResourceContainer;
class AudioBusInterface;
class TransportLoop;

/**
 * @class AudioTrackContainer
 * @brief Manages a collection of audio tracks and their associated resources.
 *
 * The `AudioTrackContainer` class provides functionality for creating, managing, and
 * interacting with audio tracks. It supports undo/redo operations, selection management,
 * serialization, and broadcasting changes to listeners.
 */
class AudioTrackContainer : public juce::ActionBroadcaster, public juce::ChangeBroadcaster, public Streamable
{
public:
    /**
     * @brief Constructs an `AudioTrackContainer` instance.
     * @param undoManager_ Shared pointer to the undo manager.
     * @param tempoProvider_ Shared pointer to the tempo provider.
     * @param audioResourceContainer_ Shared pointer to the audio resource container.
     * @param transportSourceContainer_ Shared pointer to the transport source container.
     * @param selectionManager_ Shared pointer to the selection manager.
     * @param audioBusInterface_ Shared pointer to the audio bus interface.
     * @param transportLoop_ Shared pointer to the transport loop.
     */
    AudioTrackContainer(std::shared_ptr<juce::UndoManager> undoManager_,
                        std::shared_ptr<TempoProvider> tempoProvider_,
                        std::shared_ptr<AudioResourceContainer> audioResourceContainer_,
                        std::shared_ptr<TransportSourceContainer> transportSourceContainer_,
                        std::shared_ptr<SelectionManager> selectionManager_,
                        std::shared_ptr<AudioBusInterface> audioBusInterface_,
                        std::shared_ptr<TransportLoop> transportLoop_) :
        audioBusInterface(audioBusInterface_),
        undoManager(undoManager_),
        tempoProvider(tempoProvider_),
        audioResourceContainer(audioResourceContainer_),
        transportSourceContainer(transportSourceContainer_),
        selectionManager(selectionManager_),
        transportLoop(transportLoop_),
        audioRegionAdapter(*this)
    {
    }

    /**
     * @brief Destructor for `AudioTrackContainer`.
     */
    ~AudioTrackContainer() override;

    /**
     * @brief Sets the master gain for the container.
     * @param newGain The new master gain value.
     */
    void setMasterGain(const float newGain);

    /**
     * @brief Gets the current master gain.
     * @return The master gain value.
     */
    const float getMasterGain() const noexcept;

    /**
     * @brief Checks if a group ID exists in the container.
     * @param groupId The group ID to check.
     * @return True if the group ID exists, false otherwise.
     */
    bool groupIdExists(const int groupId) const;

    /**
     * @brief Creates a new audio track.
     * @param nameString The name of the new audio track.
     * @return A shared pointer to the created `AudioTrack`.
     */
    std::shared_ptr<AudioTrack> createNewAudioTrack(const juce::String nameString);

    /**
     * @brief Cleans up resources associated with the container.
     */
    void cleanup();

    /**
     * @brief Deletes an audio track.
     * @param track Pointer to the `AudioTrack` to delete.
     * @return True if the track was successfully deleted, false otherwise.
     */
    bool deleteAudioTrack(AudioTrack* track);

    /**
     * @brief Deletes an audio track.
     * @param track Shared pointer to the `AudioTrack` to delete.
     * @return True if the track was successfully deleted, false otherwise.
     */
    bool deleteAudioTrack(std::shared_ptr<AudioTrack> track);

    /**
     * @brief Deletes all selected objects in the container.
     */
    void deleteSelectedObjects();

    /**
     * @brief Deletes unused audio regions in the container.
     */
    void deleteUnusedRegions();

    /**
     * @brief Writes the container data to a stream.
     * @param outputStream The output stream to write to.
     * @return True if the operation succeeds, false otherwise.
     */
    bool writeToStream(juce::OutputStream& outputStream) override;

    /**
     * @brief Reads the container data from a stream.
     * @param inputStream The input stream to read from.
     * @param rebuild Whether to rebuild the container during reading.
     * @return True if the operation succeeds, false otherwise.
     */
    bool readFromStream(juce::InputStream& inputStream, bool rebuild) override;

    /**
     * @brief Writes the container data to a JSON object.
     * @param output The JSON object to write to.
     * @return True if the operation succeeds, false otherwise.
     */
    bool writeToJson(json& output) override;

    /**
     * @brief Reads the container data from a JSON object.
     * @param input The JSON object to read from.
     * @param rebuild Whether to rebuild the container during reading.
     * @return True if the operation succeeds, false otherwise.
     */
    bool readFromJson(json& input, bool rebuild) override;

    /**
     * @brief Gets the size of the container in units.
     * @return The size of the container in units.
     */
    int getSizeInUnits() override;

    /**
     * @brief Gets the currently selected audio track.
     * @return A shared pointer to the selected `AudioTrack`.
     */
    std::shared_ptr<AudioTrack> getSelectedGroup() const { return audioTracks[selectedGroup]; }
    
    int getNumItems() const { return static_cast<int>(audioTracks.size());}
    std::shared_ptr<AudioTrack> getAudioTrack(int index) const;
    int getAudioTrackId(std::shared_ptr<const AudioTrack> searchTrack) const;
    int getChannelOffset(std::shared_ptr<const AudioTrack> searchTrack) const;
    
    std::shared_ptr<AudioTrack> getDefaultGroup() const;
    
    const std::vector<std::shared_ptr<AudioTrack>> &getAudioTracks() const { return audioTracks; }
    
    void selectAllGroups(bool bSelected, bool selectChildren);
    juce::SparseSet<int> getSelectedRows() const;
    void setSelectedRows(juce::SparseSet<int>& selectedRows);
    
    std::shared_ptr<TempoProvider> getTempoProvider() const noexcept { return tempoProvider; }
    std::shared_ptr<juce::UndoManager> getUndoManager() const noexcept { return undoManager; }
    std::shared_ptr<TransportSourceContainer> getTransportSourceContainer() const noexcept { return transportSourceContainer; }
    std::shared_ptr<SelectionManager> getSelectionManager() const noexcept { return selectionManager; }
    std::shared_ptr<TransportLoop> getTransportLoop() const noexcept { return transportLoop; }
    
    AudioRegionAdapter &getAudioRegionAdapter() { return audioRegionAdapter; }
    
    /**
     * @brief Gets the total number of audio track channels in the container.
     * @return The number of audio track channels.
     */
    int getNumAudioTrackChannels() const;

    /**
     * @brief Checks if any channel in the container is soloed.
     * @return True if any channel is soloed, false otherwise.
     */
    bool anyChannelSolo() const;

    /**
     * @brief Gets a new color for an audio track.
     * @return The new color for the audio track.
     */
    juce::Colour getNewAudioTrackColour() const;

    /**
     * @brief Copies selected channels to a new track.
     * @param copyChannels Whether to copy the channels.
     */
    bool copySelectedChannelsToNewTrack(bool copyChannels = true);

    /**
     * @brief Adds audio files to the container.
     * @param filenames The filenames of the audio files to add.
     * @param positionClocks The position in transport clocks to add the files.
     * @param callback A callback function for handling errors or progress.
     * @return True if the files were successfully added, false otherwise.
     */
    bool addAudioFiles(const juce::StringArray& filenames,
                       double positionClocks,
                       std::function<void (std::string)> callback,
                       bool undo);
    
    std::shared_ptr<AudioBusInterface> audioBusInterface; ///< Shared pointer to the audio bus interface.
    std::vector<std::shared_ptr<AudioTrack>> audioTracks; ///< Vector of shared pointers to audio tracks.
    
private:
    std::shared_ptr<juce::UndoManager> undoManager; ///< Shared pointer to the undo manager.
    std::shared_ptr<TempoProvider> tempoProvider; ///< Shared pointer to the tempo provider.
    std::shared_ptr<AudioResourceContainer> audioResourceContainer; ///< Shared pointer to the audio resource container.
    std::shared_ptr<TransportSourceContainer> transportSourceContainer; ///< Shared pointer to the transport source container.
    std::shared_ptr<SelectionManager> selectionManager; ///< Shared pointer to the selection manager.
    std::shared_ptr<TransportLoop> transportLoop; ///< Shared pointer to the transport loop.

    std::size_t selectedGroup = 0; ///< Index of the currently selected group.
    float masterGain = 1.f; ///< Master gain value.

    AudioRegionAdapter audioRegionAdapter; ///< Audio region adapter for managing regions.

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioTrackContainer)
};

} // namespace audium

