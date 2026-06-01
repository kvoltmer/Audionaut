//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <memory>
#include <JuceHeader.h>

#include "Engine/Streamable.h"
#include "Engine/TimeContext.h"
#include "Engine/Export/ExportAudioConfig.h"

namespace audium {

class AudioTrackContainer;
class AudioTrack;
class PlayListContainer;
class AudioRegionContainer;
class AudioResourceContainer;
class TransportSourceContainer;
class PlayListScheduler;
class LinkAudioDevice;
class AudioBusInterface;
class RecordingActionHandler;

/**
 * @class AudiumEngine
 * @brief The core engine for managing audio tracks, resources, and playback in the Audionaut application.
 *
 * The `AudiumEngine` class provides functionality to manage audio tracks, resources, playlists,
 * and playback. It also handles project file operations, undo management, and audio device configuration.
 */
class AudiumEngine : public audium::Streamable
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
     */
    AudiumEngine(std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager_,
                 std::shared_ptr<AudioTrackContainer> audioTrackContainer_,
                 std::shared_ptr<AudioResourceContainer> audioResourceContainer_,
                 std::shared_ptr<PlayListScheduler> playListScheduler_,
                 std::shared_ptr<LinkAudioDevice> linkAudioDevice_,
                 std::shared_ptr<juce::UndoManager> undoManager_,
                 std::shared_ptr<AudioBusInterface> audioBusInterface_,
                 std::shared_ptr<RecordingActionHandler> recordingActionHandler_) :
        audioDeviceManager(audioDeviceManager_),
        audioTrackContainer(audioTrackContainer_),
        audioResourceContainer(audioResourceContainer_),
        playListScheduler(playListScheduler_),
        linkAudioDevice(linkAudioDevice_),
        undoManager(undoManager_),
        audioBusInterface(audioBusInterface_),
        recordingActionHandler(recordingActionHandler_)
    {
    }

    /**
     * @brief Destructor for `AudiumEngine`.
     */
    ~AudiumEngine() override;
    
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
     * @brief Writes the engine state to a stream.
     * @param outputStream The output stream to write to.
     * @return True if the state was successfully written, false otherwise.
     */
    bool writeToStream(juce::OutputStream& outputStream) override;

    /**
     * @brief Reads the engine state from a stream.
     * @param inputStream The input stream to read from.
     * @param rebuild Whether to rebuild the engine state after reading.
     * @return True if the state was successfully read, false otherwise.
     */
    bool readFromStream(juce::InputStream& inputStream, bool rebuild) override;

    /**
     * @brief Writes the engine state to a JSON object.
     * @param output The JSON object to write to.
     * @return True if the state was successfully written, false otherwise.
     */
    bool writeToJson(json& output) override;

    /**
     * @brief Reads the engine state from a JSON object.
     * @param input The JSON object to read from.
     * @param rebuild Whether to rebuild the engine state after reading.
     * @return True if the state was successfully read, false otherwise.
     */
    bool readFromJson(json& input, bool rebuild) override;

    /**
     * @brief Retrieves the size of the engine state in units.
     * @return The size of the engine state in units.
     */
    int getSizeInUnits() override;

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
    
    // static const members for project file handling
    static const char* projectFileExtension;
    static const char* projectFileName;
    
    // static helpers to get project file paths
    static juce::File projectDirectory;
    static juce::File tempDirectory;
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

    //==============================================================================
    /**
     * @brief JUCE macro to prevent copying and detect memory leaks.
     */
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudiumEngine)
};

} // namespace audium
