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
 * The `AudiumEngine` class provides functionality to manage audio tracks, resources, playlists,
 * and playback. It also handles project file operations, undo management, and audio device configuration.
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
     * @param projectFileStore_ A shared pointer to the `ProjectFileStore` for file-level persistence.
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
     * @brief Cleans up the engine, preparing it for shutdown.
     */
    void cleanup();

    /**
     * @brief Creates a new project with default settings.
     */
    void createNewProject(const int numChannels = 2);

    /**
     * @brief Opens a project file.
     * @param file The project file to open.
     * @param callback A callback function to handle errors or status messages.
     * @return True if the file was successfully opened, false otherwise.
     */
    bool openFile(juce::File file, std::function<void(std::string)> callback);

    /**
     * @brief Saves the current project to a file.
     * @param file The file to save the project to.
     * @param callback A callback function to handle errors or status messages.
     * @return True if the file was successfully saved, false otherwise.
     */
    bool saveFile(const juce::File& file, std::function<void(std::string)> callback);

    /**
     * @brief Applies a full project JSON to the running engine without clearing
     *        undo history, the current project file, or the temp directory.
     *
     * Unlike `readFromJson`, this performs a non-destructive in-place read
     * (falling back to a rebuild when the structure differs) so it can be used
     * from an undoable action while audio is running.
     *
     * @param input The full project JSON (root key "audium").
     * @param preserveUiState True to keep the in-memory UI state instead of
     *        adopting the one from `input`.
     * @return True if the state was successfully applied, false otherwise.
     */
    bool applyProjectJson(json& input, bool preserveUiState);

    /**
     * @brief Reloads the current project file from disk as an undoable action.
     *
     * The pre-reload in-memory state becomes the undo step, so Undo restores
     * the session exactly as it was before the external change was applied.
     * The file on disk is never touched by undo/redo of this action.
     *
     * @param callback A callback function to handle errors or status messages.
     * @return True if the project was reloaded, false otherwise.
     */
    bool reloadFromDisk(std::function<void(std::string)> callback);

    /**
     * @brief Reloads only the analysis cache from disk (an external writer ran
     *        `analyze`). Derived data: touches neither the session state, the
     *        undo history, nor the external-change marker.
     */
    void reloadAnalysisFromDisk();

    /**
     * @brief Applies the package's Autosave.json as an undoable action, so a
     *        restored session starts dirty and Undo returns to the saved state.
     * @param callback A callback function to handle errors or status messages.
     * @return True if the snapshot was applied, false otherwise.
     */
    bool restoreAutosave(std::function<void(std::string)> callback);

    /**
     * @brief Writes a crash-recovery snapshot (Autosave.json). Saved projects
     *        snapshot into their package; never-saved ones into the session's
     *        temp directory (where their recordings already live), along with
     *        a pid file so a startup scan can tell a crashed session's
     *        leftovers apart from a running instance's. Never touches
     *        Project.json, disk stamps, or undo history.
     * @return True if a snapshot was written.
     */
    bool writeAutosave();

    /**
     * @brief Deletes any crash-recovery snapshot (and pid file) in the project
     *        package and the session's temp directory.
     */
    void deleteAutosave();

    /**
     * @brief The directory autosave snapshots go to: the project package for
     *        saved projects, the session's temp directory otherwise.
     */
    static juce::File getAutosaveDirectory();

    /**
     * @brief The directory resource paths are serialized relative to: the
     *        project package for saved projects, the session's temp directory
     *        (where the audio actually lives) for never-saved ones. Computing
     *        paths against the invalid project directory of a never-saved
     *        project would produce garbage that can't be resolved on restore.
     */
    static juce::File getSerializationBaseDirectory();

    /**
     * @brief Scans the temp location for a crashed session's leftover autosave:
     *        a temp-*.audium directory (not this session's) holding an
     *        Autosave.json newer than its Project.json, whose owning process is
     *        no longer alive.
     * @return The newest such directory, or an invalid File if none.
     */
    static juce::File findOrphanedTempAutosave();

    /**
     * @brief Records the on-disk modification times of Project.json and
     *        AnalysisData.json so the app's own writes can be told apart from
     *        external (agent) writes.
     */
    void refreshDiskStamps();

    /**
     * @brief Whether Project.json changed on disk since the last stamp
     *        (also true when the file vanished).
     */
    bool projectChangedOnDisk() const;

    /**
     * @brief Whether AnalysisData.json changed on disk since the last stamp.
     */
    bool analysisChangedOnDisk() const;

    /**
     * @brief Whether the current in-memory state reflects an external
     *        (agent/CLI) change that was reloaded from disk. Follows the undo
     *        stack: undoing the reload clears it, redoing it sets it again;
     *        a save or opening a project also clears it.
     */
    bool wasChangedExternally() const { return changedExternally; }

    /**
     * @brief Sets the external-change marker; used by `UndoableReloadAction`
     *        so undo/redo of a reload moves the marker with the state.
     */
    void setChangedExternally(bool changed) { changedExternally = changed; }

    /**
     * @brief Writes the engine state to a JSON object.
     * @param output The JSON object to write to.
     * @return True if the state was successfully written, false otherwise.
     */
    bool writeToJson(json& output);

    /**
     * @brief Retrieves the current project file.
     * @return The current project file as a `juce::File`.
     */
    const juce::File getCurrentProjectFile() const { return currentProjectFile; }
    
    /**
     * @brief Checks if a file is a valid JSON project file.
     * @param file The file to check.
     * @return True if the file is a valid JSON project file, false otherwise.
     */
    static bool isJsonProjectFile (const juce::File &file);
    
    /**
     * @brief Checks if a file has a valid project structure.
     *
     * Valid project structure means: a document package (directory named .audium) that contains the project file.
     * @param file The file to check.
     * @return True if the file has a valid project structure, false otherwise.
     */
    static bool isValidProjectStructure(const juce::File &file);

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
    
    /**
     * @brief Deletes obsolete audio files from the project's audio file dir.
     */
    void deleteObsoleteAudioFiles();
    
    // static helpers to get project file paths
    static juce::File projectDirectory;
    static juce::File tempDirectory;
    static int recordingCounter;
    
private:
    /**
     * @brief Reads the engine state from a JSON object, destructively: clears
     *        tracks, resources, undo history and UI state first. Only used by
     *        `openFile`; non-destructive applies go through `applyProjectJson`.
     * @param input The JSON object to read from.
     * @param rebuild Whether to rebuild the engine state after reading.
     * @return True if the state was successfully read, false otherwise.
     */
    bool readFromJson(json& input, bool rebuild);

    /**
     * @brief Serializes the engine to `target` atomically (temp file + rename).
     * @param target The file to write.
     * @param error Receives an error message on failure.
     * @param serializedOut Receives the serialized JSON on success (optional) -
     *        callers that make the written state authoritative (saveFile)
     *        assign it to currentJson; snapshots leave currentJson alone.
     * @return True if the file was written, false otherwise.
     */
    bool writeJsonToFile(const juce::File& target, std::string& error, json* serializedOut = nullptr);

    /**
     * @brief Reads `sourceFile` and applies it via an `UndoableReloadAction`.
     * @param sourceFile The project JSON file to apply.
     * @param preserveUiState True to keep the in-memory UI state.
     * @param marksExternalChange True when applying an external (agent/CLI)
     *        change - undo/redo then moves the external-change marker.
     * @param transactionName The undo transaction name.
     * @param callback A callback function to handle errors or status messages.
     * @return True if the file was applied, false otherwise.
     */
    bool applyFileAsUndoableReload(const juce::File& sourceFile,
                                   bool preserveUiState,
                                   bool marksExternalChange,
                                   const juce::String& transactionName,
                                   std::function<void(std::string)> callback);

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
     * @brief A shared pointer to the `ProjectFileStore` for file-level
     *        persistence (atomic writes, disk stamps, autosave files).
     */
    std::shared_ptr<ProjectFileStore> projectFileStore;

    /**
     * @brief The current project file.
     */
    juce::File currentProjectFile;
    
    /**
     * @brief The current json object.
     */
    json currentJson;

    /**
     * @brief The UI state as a JSON object.
     */
    json uiState;

    /**
     * @brief See `wasChangedExternally()`.
     */
    bool changedExternally = false;

    //==============================================================================
    /**
     * @brief JUCE macro to prevent copying and detect memory leaks.
     */
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudiumEngine)
};

} // namespace audium
