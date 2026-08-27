//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace audium {

/**
 * @class ProjectFileStore
 * @brief File-level persistence services for the engine: the project file
 *        format constants, framing-aware reads, atomic writes, the disk
 *        stamps behind external-change detection, and crash-recovery
 *        snapshot (Autosave.json) management.
 *
 * Knows files only - no engine graph. `AudiumEngine` orchestrates *what* to
 * persist; this class owns *how* it reaches and leaves the disk.
 */
class ProjectFileStore
{
public:
    ProjectFileStore() = default;

    // project file format constants
    static const char* projectFileExtension;
    static const char* projectFileName;
    static const char* autosaveFileName;
    static const char* autosavePidFileName;

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
     * @param target The file to write.
     * @param content The JSON to write.
     * @param error Receives an error message on failure.
     * @return True if the file was written, false otherwise.
     */
    bool writeJsonAtomically(const juce::File& target, const json& content, std::string& error);

    // ==== disk stamps (external-change detection) ============================
    // The engine records the mtimes of its own writes here so the poller can
    // tell the app's writes apart from foreign (agent/CLI) ones.

    /**
     * @brief Records both files' current on-disk modification times.
     */
    void refreshStamps(const juce::File& projectFile, const juce::File& analysisFile);

    /**
     * @brief Records explicit modification times (captured before a read, so
     *        a write landing during a long apply is re-detected).
     */
    void setStamps(juce::Time projectMtime, juce::Time analysisMtime);

    /**
     * @brief Records only the analysis file's modification time.
     */
    void setAnalysisStamp(juce::Time analysisMtime);

    /**
     * @brief Whether `projectFile` changed on disk since the last stamp
     *        (also true when the file vanished).
     */
    bool projectChangedOnDisk(const juce::File& projectFile) const;

    /**
     * @brief Whether `analysisFile` changed on disk since the last stamp.
     */
    bool analysisChangedOnDisk(const juce::File& analysisFile) const;

    // ==== crash-recovery snapshots ==========================================

    /**
     * @brief Writes the Autosave.pid owner guard when `packageDirectory` lives
     *        under the temp root, so findOrphanedTempAutosave never claims a
     *        running instance's session (including a project restored from -
     *        and still living in - a temp package).
     */
    void writePidGuardIfTemporary(const juce::File& packageDirectory);

    /**
     * @brief Deletes any crash-recovery snapshot (and pid file) in
     *        `packageDirectory`.
     */
    void deleteAutosaveIn(const juce::File& packageDirectory);

    /**
     * @brief Scans the temp location for a crashed session's leftover
     *        autosave: a temp-*.audium directory (other than
     *        `currentTempDirectory`) holding an Autosave.json newer than its
     *        Project.json, whose owning process is no longer alive.
     * @return The newest such directory, or an invalid File if none.
     */
    static juce::File findOrphanedTempAutosave(const juce::File& currentTempDirectory);

private:
    /**
     * @brief On-disk modification times recorded after the app's own writes.
     */
    juce::Time projectFileStamp;
    juce::Time analysisFileStamp;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProjectFileStore)
};

} // namespace audium
