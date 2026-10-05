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
#include "Engine/Selection/ClipOverlayTarget.h"

namespace audium {

class AudioTrack;
class AudioResourceContainer;
class AudioRegionContainer;
class AudiumEngine;
class TempoProvider;
class AudioRegion;
class VoiceSourceContainer;
class AudioResourceContainer;
class AudioBusInterface;
class TransportLoop;
class AnalysisProvider;

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
     * @param voiceSourceContainer_ Shared pointer to the voice source container.
     * @param selectionManager_ Shared pointer to the selection manager.
     * @param audioBusInterface_ Shared pointer to the audio bus interface.
     * @param transportLoop_ Shared pointer to the transport loop.
     */
    AudioTrackContainer(std::shared_ptr<juce::UndoManager> undoManager_,
                        std::shared_ptr<TempoProvider> tempoProvider_,
                        std::shared_ptr<AudioResourceContainer> audioResourceContainer_,
                        std::shared_ptr<VoiceSourceContainer> voiceSourceContainer_,
                        std::shared_ptr<SelectionManager> selectionManager_,
                        std::shared_ptr<AudioBusInterface> audioBusInterface_,
                        std::shared_ptr<TransportLoop> transportLoop_,
                        std::shared_ptr<AnalysisProvider> analysisProvider_) :
        audioBusInterface(audioBusInterface_),
        undoManager(undoManager_),
        tempoProvider(tempoProvider_),
        audioResourceContainer(audioResourceContainer_),
        voiceSourceContainer(voiceSourceContainer_),
        selectionManager(selectionManager_),
        transportLoop(transportLoop_),
        analysisProvider(analysisProvider_),
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
     * @brief Whether the last readFromJson() replaced tracks or channels
     * instead of reading them in place.
     *
     * The UI keys its track and channel components on those objects, so a
     * read that replaced them needs a rebuild (rebuildAll); a read that kept
     * them only needs a refresh (updateAll).
     */
    bool didLastReadRebuildStructure() const { return lastReadRebuiltStructure; }

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
    
    juce::SparseSet<int> getSelectedRows() const;
    void setSelectedRows(juce::SparseSet<int>& selectedRows);
    
    std::shared_ptr<TempoProvider> getTempoProvider() const noexcept { return tempoProvider; }
    std::shared_ptr<juce::UndoManager> getUndoManager() const noexcept { return undoManager; }
    std::shared_ptr<VoiceSourceContainer> getVoiceSourceContainer() const noexcept { return voiceSourceContainer; }
    std::shared_ptr<SelectionManager> getSelectionManager() const noexcept { return selectionManager; }
    std::shared_ptr<TransportLoop> getTransportLoop() const noexcept { return transportLoop; }
    std::shared_ptr<AnalysisProvider> getAnalysisProvider() const noexcept { return analysisProvider; }

    /// Which clip currently shows an in-arrangement overlay editor (the
    /// stretch overlay). Session state with its own broadcaster - see
    /// ClipOverlayTarget.
    std::shared_ptr<ClipOverlayTarget> getClipOverlayTarget() const noexcept { return clipOverlayTarget; }
    
    AudioRegionAdapter &getAudioRegionAdapter() { return audioRegionAdapter; }
    
    /**
     * @brief Gets the total number of audio track channels in the container.
     * @return The number of audio track channels.
     */
    int getNumAudioTrackChannels() const;

    /**
     * @brief The channel count as published to the audio thread.
     *
     * getNumAudioTrackChannels() walks the track and channel vectors, which
     * the message thread grows and shrinks (add/remove track or channel,
     * undo replay, project reload) - the audio callback must never iterate
     * them. It reads this atomic instead, which every channel-count change
     * republishes (see publishNumAudioTrackChannels). Any thread, lock-free.
     */
    int getPublishedNumAudioTrackChannels() const noexcept
    {
        return publishedNumAudioTrackChannels.load(std::memory_order_acquire);
    }

    /**
     * @brief Republishes getNumAudioTrackChannels() for the audio thread.
     *
     * Message thread. Called by every path that changes a track's channel
     * count (AudioTrack::addChannel/deleteChannel/cleanup, track removal,
     * cleanup); a missed call only leaves the bus one block stale, never
     * outside [0, MAX_AUDIO_CHANNELS].
     */
    void publishNumAudioTrackChannels() noexcept;

    /**
     * @brief Re-sends the mixer state of the track at `firstTrack` and of
     *        every track after it to the bus.
     *
     * The bus keeps mixer state per bus channel (track channel offset +
     * channel), so adding or removing a channel or track moves every later
     * channel to another bus channel - its gain, pan, mute and solo have to
     * follow it there, or the channel now at the old index plays with them.
     * Bus channels from the current channel count up to `previousNumChannels`
     * are reset to defaults: a channel that takes one over later must not
     * inherit a removed channel's state (a stale solo silences every other
     * channel). Message thread.
     */
    void commitChannelLayout(std::size_t firstTrack, int previousNumChannels);

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

    /** How importAudioFiles() lays out more than one file. */
    enum class ImportPlacement
    {
        separateTracks,     ///< one new track per file, all starting at the position
        stackedChannels,    ///< one new track, the files become channels of one clip
        backToBack          ///< one new track, the files follow each other
    };

    /**
     * @brief Imports audio files as one undo step, into new tracks or an existing one.
     * @param filenames The audio files to import.
     * @param positionClocks Where the (first) clip starts, in transport clocks.
     * @param placement How several files are laid out; ignored for a single file.
     * @param callback Receives the files that could not be imported, if any.
     * @param targetTrack An existing track to import into instead of new tracks;
     *        separateTracks then falls back to stackedChannels.
     * @return The number of files imported.
     */
    int importAudioFiles(const juce::StringArray& filenames,
                         double positionClocks,
                         ImportPlacement placement,
                         std::function<void (std::string)> callback,
                         std::shared_ptr<AudioTrack> targetTrack = nullptr);
    
    std::shared_ptr<AudioBusInterface> audioBusInterface; ///< Shared pointer to the audio bus interface.
    std::shared_ptr<ClipOverlayTarget> clipOverlayTarget = std::make_shared<ClipOverlayTarget>();

    std::vector<std::shared_ptr<AudioTrack>> audioTracks; ///< Vector of shared pointers to audio tracks.
    
private:
    std::shared_ptr<juce::UndoManager> undoManager; ///< Shared pointer to the undo manager.
    std::shared_ptr<TempoProvider> tempoProvider; ///< Shared pointer to the tempo provider.
    std::shared_ptr<AudioResourceContainer> audioResourceContainer; ///< Shared pointer to the audio resource container.
    std::shared_ptr<VoiceSourceContainer> voiceSourceContainer; ///< Shared pointer to the voice source container.
    std::shared_ptr<SelectionManager> selectionManager; ///< Shared pointer to the selection manager.
    std::shared_ptr<TransportLoop> transportLoop; ///< Shared pointer to the transport loop.
    std::shared_ptr<AnalysisProvider> analysisProvider; ///< Shared analysis provider (owns the analysis cache).

    std::size_t selectedGroup = 0; ///< Index of the currently selected group.
    float masterGain = 1.f; ///< Master gain value.
    std::atomic<int> publishedNumAudioTrackChannels { 0 }; ///< Channel count as the audio thread reads it.
    bool lastReadRebuiltStructure = false; ///< Set by readFromJson(): tracks or channels were replaced.

    AudioRegionAdapter audioRegionAdapter; ///< Audio region adapter for managing regions.

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioTrackContainer)
};

} // namespace audium

