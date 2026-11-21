//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <functional>
#include <memory>
#include <JuceHeader.h>
#include <nlohmann/json.hpp>

#include "Engine/Project/ProjectDirectories.h"

using json = nlohmann::json;

namespace audium {

class AudioTrackContainer;
class AudioResourceContainer;
class PlayListScheduler;
class ProjectSerializer;
struct AgentEditBatch;

/**
 * @class ProjectFileStore
 * @brief Project persistence: the file format constants and paths, the file
 *        primitives (framed reads, atomic writes, disk stamps, crash-recovery
 *        snapshots), and the session orchestration (open/save/autosave/reload)
 *        that used to live on `AudiumEngine`.
 *
 * `ProjectSerializer` owns what the document JSON contains and how it is
 * applied to the graph; this class owns everything about how and what reaches
 * disk, plus the persistence session state (currentProjectFile, the
 * external-change marker, the engine's ProjectDirectories).
 *
 * Wired in `AudiumFactory`: constructed with the graph collaborators, then
 * given the serializer via `setSerializer()` (the serializer holds this store
 * weakly - see ProjectSerializer).
 */
class ProjectFileStore
{
public:
    ProjectFileStore(std::shared_ptr<AudioTrackContainer> audioTrackContainer_,
                     std::shared_ptr<AudioResourceContainer> audioResourceContainer_,
                     std::shared_ptr<PlayListScheduler> playListScheduler_,
                     std::shared_ptr<juce::UndoManager> undoManager_,
                     std::shared_ptr<ProjectDirectories> projectDirectories_);

    // project file format constants
    static const char* projectFileExtension;
    static const char* projectFileName;
    static const char* autosaveFileName;
    static const char* autosavePidFileName;

    /**
     * @brief The project file format version, written as "file_version" and
     *        asserted on load.
     */
    static constexpr int fileVersion = 1;

    /** The engine's project and temp directories (shared with the resource container). */
    std::shared_ptr<ProjectDirectories> getProjectDirectories() const { return projectDirectories; }

    /** The package of the open project; invalid while it was never saved. */
    juce::File getProjectDirectory() const { return projectDirectories->getProjectDirectory(); }

    /** The session's temp package: audio of a never-saved project, crash-recovery snapshots. */
    juce::File getTemporaryDirectory() const { return projectDirectories->getTemporaryDirectory(); }

    /**
     * @brief The directory resource paths are serialized relative to: the
     *        project package for saved projects, the session's temp directory
     *        (where the audio actually lives) for never-saved ones.
     */
    juce::File getSerializationBaseDirectory() const;

    /**
     * @brief The directory autosave snapshots go to: the project package for
     *        saved projects, the session's temp directory otherwise.
     */
    juce::File getAutosaveDirectory() const;

    /**
     * @brief Whether a process with this id is still running.
     *
     * Used to tell a live owner from the leftovers of a crashed one, both for
     * autosave pid guards and for the agent host marker.
     */
    static bool isProcessAlive(int pid);

    /**
     * @brief Checks if a file is an explicit JSON project file
     *        (foo.json, or legacy foo.audium).
     */
    static bool isJsonProjectFile(const juce::File& file);

    /**
     * @brief The crash-recovery snapshot beside `projectFile`.
     */
    static juce::File autosaveFileFor(const juce::File& projectFile);

    /**
     * @brief Whether reloading `projectFile` would discard unsaved edits.
     *
     * The crash-recovery snapshot is the only on-disk trace of unsaved work,
     * so its timestamp dates them: a snapshot newer than the incoming file
     * means the writer never saw the edits it is about to overwrite, and the
     * reload needs the user's consent.
     */
    static bool reloadWouldLoseEdits(const juce::File& projectFile, bool hasUnsavedEdits);

    /**
     * @brief Checks if a file is a valid document package:
     *        a directory named *.audium containing Project.json.
     */
    static bool isValidProjectStructure(const juce::File& file);

    /** What to tell the user when the chosen save location isn't writable. */
    static std::string noWriteAccessMessage();

    /**
     * @brief Reads a project JSON file (Streamable::readJsonText, so files
     *        from before 1.6.6 with a trailing NUL still load).
     * @throws std::runtime_error when the file can't be opened, is empty, or
     *         doesn't parse.
     */
    static json readProjectJson(const juce::File& file);

    /**
     * @brief Writes JSON to `target` atomically (temp file + rename) as plain
     *        JSON text (Streamable::writeJsonText).
     */
    bool writeJsonAtomically(const juce::File& target, const json& content, std::string& error);

    // ==== session orchestration =============================================

    /**
     * @brief Connects the store to the document serializer. Called once by
     *        `AudiumFactory` (the serializer is constructed after the store).
     */
    void setSerializer(std::shared_ptr<ProjectSerializer> serializer_);

    /**
     * @brief Opens a file: an empty file is a no-op (user canceled), a known
     *        audio format is imported into the current project, anything else
     *        is loaded as a project. On a failed project load the engine falls
     *        back to a fresh project.
     * @return True on success (or no-op), false otherwise.
     */
    bool open(juce::File file, std::function<void(std::string)> callback);

    /**
     * @brief Saves the current project to `file`, relocating audio files and
     *        the analysis sidecar as needed. Clears undo history and any
     *        crash-recovery snapshots (including the previous package's after
     *        a Save As).
     *
     * A save is all-or-nothing: on failure the callback gets the reason,
     * false is returned, the session still points at its previous package
     * (whose audio is never moved, only copied), and whatever was created in
     * the target package is removed again.
     */
    bool save(const juce::File& file, std::function<void(std::string)> callback);

    /**
     * @brief Writes a crash-recovery snapshot (Autosave.json). Saved projects
     *        snapshot into their package; never-saved ones into the session's
     *        temp directory, with a pid guard. Never touches Project.json,
     *        disk stamps, or undo history.
     */
    bool writeAutosave();

    /**
     * @brief Deletes any crash-recovery snapshot (and pid file) in the
     *        project package and the session's temp directory.
     */
    void deleteAutosave();

    /**
     * @brief Applies the package's Autosave.json as an undoable action, so a
     *        restored session starts dirty and Undo returns to the saved state.
     */
    bool restoreAutosave(std::function<void(std::string)> callback);

    /**
     * @brief Applies `afterState` to the running document as one undoable
     *        action; the pre-apply in-memory state becomes the undo step.
     *        Touches no file.
     *
     * The state-based counterpart of the reload paths: it is how an edit made
     * somewhere other than this engine - an agent's, computed elsewhere -
     * reaches the document without a save round-trip. `marksExternalChange`
     * sets the agent marker that the window title shows.
     */
    bool applyStateAsUndoableReload(json afterState,
                                    bool preserveUiState,
                                    bool marksExternalChange,
                                    const juce::String& transactionName,
                                    std::function<void(std::string)> callback);

    /**
     * @brief What the edits of one agent task have in common. Both parts
     *        travel with the hosted command; either may be empty.
     */
    struct AgentEditGroup
    {
        juce::String session; ///< the agent session (one MCP server process); empty for a bare CLI call
        juce::String task;    ///< the task the agent named its edits with, if any

        /// What the edits have in common: the task within its session, or
        /// the session itself when the agent names no task
        juce::String key() const { return task.isEmpty() ? session : session + "/" + task; }
    };

    /**
     * @brief Applies an agent verb's result to the running document, as part
     *        of the current agent batch when it continues one, else as a new
     *        undo step. Touches no file; sets the external-change marker.
     *
     * A verb extends the current batch when it belongs to the same group,
     * the batch's step is still the last one in the undo history (no undo,
     * no other edit's step since, nothing left to redo), the document is
     * the one the batch left (no user edit since) and the batch saw an
     * edit within `agentBatchIdleMinutes`. Extending applies the state
     * directly and moves the batch's redo state; nothing is added to the
     * undo history. Otherwise a batch starts: one `UndoableAgentBatchAction`,
     * named after the task ("Agent: <task>") or "Agent edits". A session
     * that names no task groups by session: its verbs share one step until
     * a task is named or something else ends the batch. A group without a
     * session (a bare CLI call, an older MCP server) never extends: each of
     * its verbs is a step of its own.
     *
     * The user's view and clip selection win over the verb's: the applied
     * state carries the live `ui_state` and `selected` flags.
     *
     * @param liveState The document as it is now, when the caller already
     *                  serialised it; the other overload does so itself.
     */
    bool applyAgentState(json afterState,
                         json liveState,
                         const AgentEditGroup& group,
                         std::function<void(std::string)> callback);
    bool applyAgentState(json afterState,
                         const AgentEditGroup& group,
                         std::function<void(std::string)> callback);

    /**
     * @brief The state minus what moves without the user editing anything:
     *        the playhead while playing, scroll and zoom, the clip
     *        selection, the insert editors' windows. Two states that agree
     *        on this are the same document.
     */
    static json documentOf(json state);

    /** A batch with no edit for this long closes; the next verb starts a new step. */
    static constexpr int agentBatchIdleMinutes = 10;

    /** Tests: the clock `applyAgentState` reads for the idle limit. */
    void setClock(std::function<juce::Time()> clock_) { clock = std::move(clock_); }

    /**
     * @brief Reloads the current project file from disk as an undoable action
     *        (external/agent change). The pre-reload in-memory state becomes
     *        the undo step; undo/redo never touches the file on disk.
     */
    bool reloadFromDisk(std::function<void(std::string)> callback);

    /**
     * @brief Reloads only the analysis cache from disk (an external writer ran
     *        `analyze`). Derived data: touches neither the session state, the
     *        undo history, nor the external-change marker.
     */
    void reloadAnalysisFromDisk();

    /**
     * @brief Records the on-disk modification times of the current project
     *        file and analysis sidecar, so the app's own writes can be told
     *        apart from external (agent) writes.
     */
    void refreshDiskStamps();

    /**
     * @brief Whether the current project file changed on disk since the last
     *        stamp (false when no project file is open).
     */
    bool projectChangedOnDisk() const;

    /**
     * @brief Whether the analysis sidecar changed on disk since the last
     *        stamp (false when no project file is open).
     */
    bool analysisChangedOnDisk() const;

    /**
     * @brief The currently open project file (invalid for never-saved
     *        projects).
     */
    juce::File getCurrentProjectFile() const { return currentProjectFile; }

    /**
     * @brief Whether the current in-memory state reflects an external
     *        (agent/CLI) change that was reloaded from disk. Follows the undo
     *        stack; a save or opening a project clears it.
     */
    bool wasChangedExternally() const { return changedExternally; }

    /**
     * @brief Sets the external-change marker; used by `UndoableReloadAction`
     *        so undo/redo of a reload moves the marker with the state.
     */
    void setChangedExternally(bool changed) { changedExternally = changed; }

    /**
     * @brief Whether something that is saved with the project but is no
     *        edit changed since the project was opened or last saved: an
     *        insert editor's window was moved, opened or closed. No undo
     *        step stands for it, so the undo history does not show it.
     */
    bool hasViewChanges() const { return viewChanged; }

    /**
     * @brief Says that such a thing changed. The undo manager's listeners
     *        hear of the first one, as they hear of an edit (the window
     *        title follows).
     */
    void markViewChanged();

    /**
     * @brief Whether there is anything to save: an edit in the undo
     *        history, or a view change (`hasViewChanges()`).
     */
    bool hasUnsavedChanges() const;


    /**
     * @brief The audio files in the package that the saved project file
     *        doesn't reference; empty before the first save.
     */
    std::vector<juce::File> findObsoleteAudioFiles() const;

    /**
     * @brief Deletes audio files in the package that the saved project file
     *        no longer references (with a confirmation prompt).
     */
    void deleteObsoleteAudioFiles();

    /**
     * @brief Clears the persistence session state (current file, authoritative
     *        JSON, external-change marker). Called by `AudiumEngine::cleanup`;
     *        deliberately does not touch the engine (teardown-safe).
     */
    void closeProject();

    /**
     * @brief Scans the temp location for a crashed session's leftover
     *        autosave owned by a no-longer-running process.
     * @return The newest such directory, or an invalid File if none.
     */
    juce::File findOrphanedTempAutosave() const;
    static juce::File findOrphanedTempAutosave(const juce::File& currentTempDirectory);

    // ==== file primitives (stamps by explicit file, pid/autosave files) =====

    void refreshStamps(const juce::File& projectFile, const juce::File& analysisFile);
    void setStamps(juce::Time projectMtime, juce::Time analysisMtime);
    void setAnalysisStamp(juce::Time analysisMtime);
    bool projectChangedOnDisk(const juce::File& projectFile) const;
    bool analysisChangedOnDisk(const juce::File& analysisFile) const;

    /**
     * @brief Writes the Autosave.pid owner guard when `packageDirectory` lives
     *        under the temp root, so findOrphanedTempAutosave never claims a
     *        running instance's session.
     */
    void writePidGuardIfTemporary(const juce::File& packageDirectory);

    /**
     * @brief Deletes any crash-recovery snapshot (and pid file) in
     *        `packageDirectory`.
     */
    void deleteAutosaveIn(const juce::File& packageDirectory);

private:
    /**
     * @brief Serializes the engine and writes it to `target` atomically.
     */
    bool writeJsonToFile(const juce::File& target, std::string& error);

    /**
     * @brief Reads `sourceFile` and applies it to the engine via an
     *        `UndoableReloadAction`.
     */
    bool applyFileAsUndoableReload(const juce::File& sourceFile,
                                   bool preserveUiState,
                                   bool marksExternalChange,
                                   const juce::String& transactionName,
                                   std::function<void(std::string)> callback);

    std::shared_ptr<AudioTrackContainer> audioTrackContainer;
    std::shared_ptr<AudioResourceContainer> audioResourceContainer;
    std::shared_ptr<PlayListScheduler> playListScheduler;
    std::shared_ptr<juce::UndoManager> undoManager;
    std::shared_ptr<ProjectDirectories> projectDirectories;

    /**
     * @brief The document serializer (set by the factory after construction;
     *        the serializer holds this store weakly).
     */
    std::shared_ptr<ProjectSerializer> serializer;

    /**
     * @brief The currently open project file.
     */
    juce::File currentProjectFile;


    /**
     * @brief See `wasChangedExternally()`.
     */
    bool changedExternally = false;

    /**
     * @brief See `hasViewChanges()`.
     */
    bool viewChanged = false;

    /// Whether `group`'s verb may extend `batch` rather than start a step.
    /// `liveDocument` is `documentOf` the document as it is now.
    bool canExtendAgentBatch(const AgentEditBatch& batch,
                             const AgentEditGroup& group,
                             const json& liveDocument,
                             juce::Time now) const;

    /// The agent batch that the last applied verb belongs to, if any. The
    /// undo step owns it: once the undo manager drops the step, this expires.
    std::weak_ptr<AgentEditBatch> currentAgentBatch;

    std::function<juce::Time()> clock = [] { return juce::Time::getCurrentTime(); };

    /**
     * @brief On-disk modification times recorded after the app's own writes.
     */
    juce::Time projectFileStamp;
    juce::Time analysisFileStamp;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProjectFileStore)
};

} // namespace audium
