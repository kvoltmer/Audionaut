//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <functional>
#include <memory>
#include <JuceHeader.h>

#include "Engine/AudiumEngine.h"
#include "Engine/Project/ProjectFileStore.h"

namespace audium
{

/**
 * @class ProjectMonitor
 * @brief Watches the open project package for external (agent) writes and
 *        drives the autosave cadence.
 *
 * A 1 s message-thread timer polls the modification times of Project.json and
 * AnalysisData.json by path (the CLI/MCP writers replace the files via
 * temp-file + rename, so inode-bound watchers would be orphaned). The engine's
 * disk stamps tell the app's own writes apart from foreign ones. A foreign
 * change is reported only once its mtimes have been stable for one tick, which
 * absorbs the writers' sequential Project.json / AnalysisData.json pair.
 *
 * Autosave is edit-driven: the undo manager broadcasts on every performed,
 * undone or redone action, and the snapshot is written on the next timer tick,
 * so bursts of edits coalesce into at most one write per second. An edit that
 * leaves the session clean deletes the snapshot instead.
 */
class ProjectMonitor : private juce::Timer,
                       private juce::ChangeListener
{
public:
    /**
     * @brief Constructs a `ProjectMonitor` and starts polling.
     * @param engine_ The engine whose project file is watched.
     */
    explicit ProjectMonitor(std::shared_ptr<AudiumEngine> engine_);

    /**
     * @brief Destructor for `ProjectMonitor`.
     */
    ~ProjectMonitor() override;

    /**
     * @brief Called when a foreign on-disk change has settled.
     */
    std::function<void()> onExternalChange;

    /**
     * @brief Called once when the open project file vanished from disk.
     */
    std::function<void()> onProjectFileMissing;

    /**
     * @brief Called after an edit when the dirty project is due for a
     *        crash-recovery snapshot.
     */
    std::function<void()> onAutosaveDue;

    /**
     * @brief Suspends/resumes polling, e.g. while the app itself is loading
     *        or saving.
     */
    void setSuspended(bool shouldSuspend) { suspended = shouldSuspend; }

private:
    void timerCallback() override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    std::shared_ptr<AudiumEngine> engine;
    std::shared_ptr<ProjectFileStore> store;

    bool suspended = false;
    bool missingReported = false;

    // settle logic for foreign changes
    bool changePending = false;
    juce::Time pendingProjectMtime;
    juce::Time pendingAnalysisMtime;

    // set by undo-manager broadcasts, consumed on the next tick
    bool autosavePending = false;

    static constexpr int intervalMilliseconds = 1000;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProjectMonitor)
};

} // namespace audium
