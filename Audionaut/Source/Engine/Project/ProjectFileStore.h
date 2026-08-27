//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <functional>
#include <memory>
#include <JuceHeader.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace audium {

class AudioTrackContainer;
class AudioResourceContainer;
class PlayListScheduler;
class ProjectSerializer;

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
 * authoritative currentJson, the external-change marker, the path statics).
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
                     std::shared_ptr<juce::UndoManager> undoManager_);

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

    // project file paths (session-wide)
    static juce::File projectDirectory;
    static juce::File tempDirectory;

    /**
     * @brief The directory resource paths are serialized relative to: the
     *        project package for saved projects, the session's temp directory
     *        (where the audio actually lives) for never-saved ones.
     */
    static juce::File getSerializationBaseDirectory();

    /**
     * @brief The directory autosave snapshots go to: the project package for
     *        saved projects, the session's temp directory otherwise.
     */
    static juce::File getAutosaveDirectory();

    /**
     * @brief Checks if a file is an explicit JSON project file
     *        (foo.json, or legacy foo.audium).
     */
    static bool isJsonProjectFile(const juce::File& file);

    /**
     * @brief Checks if a file is a valid document package:
     *        a directory named *.audium containing Project.json.
     */
    static bool isValidProjectStructure(const juce::File& file);

    /**
     * @brief Reads a project JSON file. The file is framed by
     *        juce::OutputStream::writeString (see Streamable), so it is read
     *        back the same way before parsing.
     * @throws std::runtime_error when the file can't be opened, is empty, or
     *         doesn't parse.
     */
    static json readProjectJson(const juce::File& file);

    /**
     * @brief Writes JSON to `target` atomically (temp file + rename) with the
     *        writeString framing.
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
     * @brief Sets the authoritative applied state; used by
     *        `UndoableReloadAction` after a successful apply. Only
     *        open/save/apply update this - snapshots never do.
     */
    void setCurrentJson(json state) { currentJson = std::move(state); }

    /**
     * @brief Deletes audio files in the package that the authoritative
     *        state no longer references (with a confirmation prompt).
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
    static juce::File findOrphanedTempAutosave();
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
     * @param serializedOut Receives the serialized JSON on success (optional).
     */
    bool writeJsonToFile(const juce::File& target, std::string& error, json* serializedOut = nullptr);

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
     * @brief The last authoritative applied/saved state; its file references
     *        drive obsolete-file cleanup.
     */
    json currentJson;

    /**
     * @brief See `wasChangedExternally()`.
     */
    bool changedExternally = false;

    /**
     * @brief On-disk modification times recorded after the app's own writes.
     */
    juce::Time projectFileStamp;
    juce::Time analysisFileStamp;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProjectFileStore)
};

} // namespace audium
