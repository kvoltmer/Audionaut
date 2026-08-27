//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <memory>
#include <JuceHeader.h>
#include <nlohmann/json.hpp>

#include "Engine/TimeContext.h"
#include "Engine/Export/ExportAudioConfig.h"

using json = nlohmann::json;

namespace audium {

class AudioTrackContainer;
class AudioTrack;
class PlayListContainer;
class AudioRegionContainer;
class AudioResourceContainer;
class VoiceSourceContainer;
class PlayListScheduler;
class LinkAudioDevice;
class AudioBusInterface;
class RecordingActionHandler;
class ProjectFileStore;

/**
 * @class AudiumEngine
 * @brief The core engine for managing audio tracks, resources, and playback in the Audionaut application.
 *
 * The `AudiumEngine` class is the document model: it owns the object graph and
 * its (de)serialization to JSON, undo management, and audio device lifecycle.
 * Everything about how a project reaches or leaves disk - open/save, autosave,
 * external-change reloads, paths and session state - lives on the injected
 * `ProjectFileStore` (reachable via `getProjectFileStore()`).
 */
class AudiumEngine
{

public:
    /**
     * @brief Constructs an `AudiumEngine` with the required dependencies.
     * @param audioDeviceManager_ A shared pointer to the `juce::AudioDeviceManager` for audio device management.
     * @param audioTrackContainer_ A shared pointer to the `AudioTrackContainer` for managing audio tracks.
     * @param audioResourceContainer_ A shared pointer to the `AudioResourceContainer` for managing audio resources.
     * @param playListScheduler_ A shared pointer to the `PlayListScheduler` for playlist scheduling.
     * @param linkAudioDevice_ A shared pointer to the `LinkAudioDevice` for audio device linking.
     * @param undoManager_ A shared pointer to the `juce::UndoManager` for undo/redo functionality.
     * @param audioBusInterface_ A shared pointer to the `AudioBusInterface` for audio bus management.
     * @param projectFileStore_ A shared pointer to the `ProjectFileStore` for project persistence.
     */
    AudiumEngine(std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager_,
                 std::shared_ptr<AudioTrackContainer> audioTrackContainer_,
                 std::shared_ptr<AudioResourceContainer> audioResourceContainer_,
                 std::shared_ptr<PlayListScheduler> playListScheduler_,
                 std::shared_ptr<LinkAudioDevice> linkAudioDevice_,
                 std::shared_ptr<juce::UndoManager> undoManager_,
                 std::shared_ptr<AudioBusInterface> audioBusInterface_,
                 std::shared_ptr<RecordingActionHandler> recordingActionHandler_,
                 std::shared_ptr<ProjectFileStore> projectFileStore_) :
        audioDeviceManager(audioDeviceManager_),
        audioTrackContainer(audioTrackContainer_),
        audioResourceContainer(audioResourceContainer_),
        playListScheduler(playListScheduler_),
        linkAudioDevice(linkAudioDevice_),
        undoManager(undoManager_),
        audioBusInterface(audioBusInterface_),
        recordingActionHandler(recordingActionHandler_),
        projectFileStore(projectFileStore_)
    {
    }

    /**
     * @brief Destructor for `AudiumEngine`.
     */
    ~AudiumEngine();

    /**
     * @brief Initializes the engine and its components.
     */
    void initialise();

    /**
     * @brief Uninitializes the engine and releases resources.
     */
    void uninitialise();

    /**
     * @brief Cleans up the engine: clears tracks, resources, undo history,
     *        UI state, and the store's persistence session state.
     */
    void cleanup();

    /**
     * @brief Creates a new project with default settings.
     */
    void createNewProject(const int numChannels = 2);

    /**
     * @brief Writes the engine state to a JSON object.
     * @param output The JSON object to write to.
     * @return True if the state was successfully written, false otherwise.
     */
    bool writeToJson(json& output);

    /**
     * @brief Reads the engine state from a JSON object, destructively: clears
     *        tracks, resources, undo history and UI state first. Intended for
     *        `ProjectFileStore::open`; non-destructive applies go through
     *        `applyProjectJson`.
     * @param input The JSON object to read from.
     * @param rebuild Whether to rebuild the engine state after reading.
     * @return True if the state was successfully read, false otherwise.
     */
    bool readFromJson(json& input, bool rebuild);

    /**
     * @brief Applies a full project JSON to the running engine without
     *        clearing undo history or the UI state. Unlike `readFromJson`,
     *        this performs a non-destructive in-place read (falling back to a
     *        rebuild when the structure differs) so it can be used from an
     *        undoable action while audio is running.
     * @param input The full project JSON (root key "audium").
     * @param preserveUiState True to keep the in-memory UI state instead of
     *        adopting the one from `input`.
     * @return True if the state was successfully applied, false otherwise.
     */
    bool applyProjectJson(json& input, bool preserveUiState);

    /**
     * @brief Retrieves the project file store owning all persistence
     *        (open/save/autosave/reload, paths, session state).
     */
    std::shared_ptr<ProjectFileStore> getProjectFileStore() const
    {
        return projectFileStore;
    }

    /**
     * @brief Retrieves the audio track container.
     * @return A shared pointer to the `AudioTrackContainer`.
     */
    std::shared_ptr<AudioTrackContainer> getAudioTrackContainer() const
    {
        return audioTrackContainer;
    }

    /**
     * @brief Retrieves the audio resource container.
     * @return A shared pointer to the `AudioResourceContainer`.
     */
    std::shared_ptr<AudioResourceContainer> getAudioResourceContainer() const
    {
        return audioResourceContainer;
    }

    /**
     * @brief Retrieves the playlist container for a given audio track.
     * @param track A shared pointer to the `AudioTrack` to retrieve the playlist container for.
     * @return A shared pointer to the `PlayListContainer`.
     */
    std::shared_ptr<PlayListContainer> getPlayListContainer(std::shared_ptr<AudioTrack> track) const;

    /**
     * @brief Retrieves the playlist scheduler.
     * @return A shared pointer to the `PlayListScheduler`.
     */
    std::shared_ptr<PlayListScheduler> getPlayListScheduler() const
    {
        return playListScheduler;
    }

    /**
     * @brief Retrieves the undo manager.
     * @return A shared pointer to the `juce::UndoManager`.
     */
    std::shared_ptr<juce::UndoManager> getUndoManager() const
    {
        return undoManager;
    }

    /**
     * @brief Retrieves the audio device manager.
     * @return A shared pointer to the `juce::AudioDeviceManager`.
     */
    std::shared_ptr<juce::AudioDeviceManager> getAudioDeviceManager() const
    {
        return audioDeviceManager;
    }

    /**
     * @brief Retrieves the audio bus interface.
     * @return A shared pointer to the `AudioBusInterface`.
     */
    std::shared_ptr<AudioBusInterface> getAudioBusInterface() const
    {
        return audioBusInterface;
    }

    std::shared_ptr<RecordingActionHandler> getRecordingActionHandler() const
    {
        return recordingActionHandler;
    }

    /**
     * @brief Retrieves the UI state as a JSON object.
     * @return A reference to the JSON object representing the UI state.
     */
    json& getUiState()
    {
        return uiState;
    }

    /**
     * @brief Sets the bypass state of the engine.
     * @param bypass True to enable bypass, false to disable it.
     */
    void setBypass(bool bypass);

    static int recordingCounter;

private:
    /**
     * @brief A shared pointer to the `juce::AudioDeviceManager` for audio device management.
     */
    std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager;

    /**
     * @brief A shared pointer to the `AudioTrackContainer` for managing audio tracks.
     */
    std::shared_ptr<AudioTrackContainer> audioTrackContainer;

    /**
     * @brief A shared pointer to the `AudioResourceContainer` for managing audio resources.
     */
    std::shared_ptr<AudioResourceContainer> audioResourceContainer;

    /**
     * @brief A shared pointer to the `PlayListScheduler` for playlist scheduling.
     */
    std::shared_ptr<PlayListScheduler> playListScheduler;

    /**
     * @brief A shared pointer to the `LinkAudioDevice` for audio device linking.
     */
    std::shared_ptr<LinkAudioDevice> linkAudioDevice;

    /**
     * @brief A shared pointer to the `juce::UndoManager` for undo/redo functionality.
     */
    std::shared_ptr<juce::UndoManager> undoManager;

    /**
     * @brief A shared pointer to the `AudioBusInterface` for audio bus management.
     */
    std::shared_ptr<AudioBusInterface> audioBusInterface;

    /**
     * @brief A shared pointer to the `RecordingActionHandler` for recording action management.
     */
    std::shared_ptr<RecordingActionHandler> recordingActionHandler;

    /**
     * @brief A shared pointer to the `ProjectFileStore` for project persistence.
     */
    std::shared_ptr<ProjectFileStore> projectFileStore;

    /**
     * @brief The UI state as a JSON object.
     */
    json uiState;

    //==============================================================================
    /**
     * @brief JUCE macro to prevent copying and detect memory leaks.
     */
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudiumEngine)
};

} // namespace audium
