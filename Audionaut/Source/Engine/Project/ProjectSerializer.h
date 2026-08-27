//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <memory>
#include <JuceHeader.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace audium {

class AudioTrackContainer;
class AudioResourceContainer;
class PlayListScheduler;
class LinkAudioDevice;
class AudioBusInterface;
class ProjectFileStore;

/**
 * @class ProjectSerializer
 * @brief The document (de)serializer: turns the live object graph into
 *        project JSON and back, and owns the document lifecycle (cleanup,
 *        default project) plus the UI-state buffer that travels with it.
 *
 * `ProjectFileStore` decides how bytes reach disk; this class decides what
 * the document JSON contains and how it is applied to the running graph
 * (destructively on open, non-destructively for undoable reloads).
 */
class ProjectSerializer
{
public:
    /**
     * @brief Constructs a `ProjectSerializer` with the graph collaborators.
     */
    ProjectSerializer(std::shared_ptr<AudioTrackContainer> audioTrackContainer_,
                      std::shared_ptr<AudioResourceContainer> audioResourceContainer_,
                      std::shared_ptr<PlayListScheduler> playListScheduler_,
                      std::shared_ptr<LinkAudioDevice> linkAudioDevice_,
                      std::shared_ptr<AudioBusInterface> audioBusInterface_,
                      std::shared_ptr<juce::UndoManager> undoManager_,
                      std::shared_ptr<ProjectFileStore> projectFileStore_);

    /**
     * @brief Writes the document state to a JSON object.
     * @param output The JSON object to write to.
     * @return True if the state was successfully written, false otherwise.
     */
    bool writeToJson(json& output);

    /**
     * @brief Reads the document state from a JSON object, destructively:
     *        clears tracks, resources, undo history and UI state first, and
     *        bypasses the audio callback for the duration of the rebuild
     *        (exception-safe). Intended for `ProjectFileStore::open`;
     *        non-destructive applies go through `applyProjectJson`.
     * @param input The JSON object to read from.
     * @param rebuild Whether to rebuild the graph after reading.
     * @return True if the state was successfully read, false otherwise.
     */
    bool readFromJson(json& input, bool rebuild);

    /**
     * @brief Applies a full project JSON to the running graph without
     *        clearing undo history or the UI state: a non-destructive
     *        in-place read (falling back to a rebuild when the structure
     *        differs), usable from an undoable action while audio runs.
     *        Stops any active recording first - the project structure must
     *        never be swapped underneath live recorders.
     * @param input The full project JSON (root key "audium").
     * @param preserveUiState True to keep the in-memory UI state instead of
     *        adopting the one from `input`.
     * @return True if the state was successfully applied, false otherwise.
     */
    bool applyProjectJson(json& input, bool preserveUiState);

    /**
     * @brief Resets the document: clears tracks, resources, undo history,
     *        UI state, and the store's persistence session state.
     */
    void cleanup();

    /**
     * @brief Creates a new default project (one track).
     */
    void createNewProject(int numChannels = 2);

    /**
     * @brief Retrieves the UI state that is serialized with the document.
     * @return A reference to the JSON object representing the UI state.
     */
    json& getUiState()
    {
        return uiState;
    }

private:
    std::shared_ptr<AudioTrackContainer> audioTrackContainer;
    std::shared_ptr<AudioResourceContainer> audioResourceContainer;
    std::shared_ptr<PlayListScheduler> playListScheduler;
    std::shared_ptr<LinkAudioDevice> linkAudioDevice;
    std::shared_ptr<AudioBusInterface> audioBusInterface;
    std::shared_ptr<juce::UndoManager> undoManager;

    /**
     * @brief The store whose session state `cleanup()` closes. Weak for
     *        teardown safety (the store holds this serializer).
     */
    std::weak_ptr<ProjectFileStore> projectFileStore;

    /**
     * @brief The UI state (view/zoom/scroll) serialized with the document.
     */
    json uiState;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProjectSerializer)
};

} // namespace audium
