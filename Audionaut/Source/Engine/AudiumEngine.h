//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <memory>
#include <JuceHeader.h>

#include "Engine/TimeContext.h"
#include "Engine/Export/ExportAudioConfig.h"

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
class ProjectSerializer;

/**
 * @class AudiumEngine
 * @brief The composition handle for the Audionaut object graph, plus the
 *        audio device lifecycle.
 *
 * The engine owns nothing document-shaped anymore: `ProjectSerializer` turns
 * the graph into project JSON and back (and owns the document lifecycle),
 * `ProjectFileStore` owns how projects reach and leave disk. What remains
 * here is the wiring the rest of the app reaches the graph through, and
 * opening/closing the audio device.
 */
class AudiumEngine
{

public:
    /**
     * @brief Constructs an `AudiumEngine` with the required dependencies.
     */
    AudiumEngine(std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager_,
                 std::shared_ptr<AudioTrackContainer> audioTrackContainer_,
                 std::shared_ptr<AudioResourceContainer> audioResourceContainer_,
                 std::shared_ptr<PlayListScheduler> playListScheduler_,
                 std::shared_ptr<LinkAudioDevice> linkAudioDevice_,
                 std::shared_ptr<juce::UndoManager> undoManager_,
                 std::shared_ptr<AudioBusInterface> audioBusInterface_,
                 std::shared_ptr<RecordingActionHandler> recordingActionHandler_,
                 std::shared_ptr<ProjectFileStore> projectFileStore_,
                 std::shared_ptr<ProjectSerializer> projectSerializer_) :
        audioDeviceManager(audioDeviceManager_),
        audioTrackContainer(audioTrackContainer_),
        audioResourceContainer(audioResourceContainer_),
        playListScheduler(playListScheduler_),
        linkAudioDevice(linkAudioDevice_),
        undoManager(undoManager_),
        audioBusInterface(audioBusInterface_),
        recordingActionHandler(recordingActionHandler_),
        projectFileStore(projectFileStore_),
        projectSerializer(projectSerializer_)
    {
    }

    /**
     * @brief Destructor: resets the document so the persistence session is
     *        closed at teardown.
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
     * @brief Retrieves the project file store owning all persistence
     *        (open/save/autosave/reload, paths, session state).
     */
    std::shared_ptr<ProjectFileStore> getProjectFileStore() const
    {
        return projectFileStore;
    }

    /**
     * @brief Retrieves the document serializer (graph <-> project JSON,
     *        document lifecycle, UI state buffer).
     */
    std::shared_ptr<ProjectSerializer> getProjectSerializer() const
    {
        return projectSerializer;
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
     * @brief Retrieves the Link audio device (the audio callback).
     * @return A shared pointer to the `LinkAudioDevice`.
     */
    std::shared_ptr<LinkAudioDevice> getLinkAudioDevice() const
    {
        return linkAudioDevice;
    }

private:
    std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager;
    std::shared_ptr<AudioTrackContainer> audioTrackContainer;
    std::shared_ptr<AudioResourceContainer> audioResourceContainer;
    std::shared_ptr<PlayListScheduler> playListScheduler;
    std::shared_ptr<LinkAudioDevice> linkAudioDevice;
    std::shared_ptr<juce::UndoManager> undoManager;
    std::shared_ptr<AudioBusInterface> audioBusInterface;
    std::shared_ptr<RecordingActionHandler> recordingActionHandler;
    std::shared_ptr<ProjectFileStore> projectFileStore;
    std::shared_ptr<ProjectSerializer> projectSerializer;

    //==============================================================================
    /**
     * @brief JUCE macro to prevent copying and detect memory leaks.
     */
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudiumEngine)
};

} // namespace audium
