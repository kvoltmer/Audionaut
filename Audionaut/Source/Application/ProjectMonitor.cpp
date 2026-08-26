//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "ProjectMonitor.h"
#include "Engine/Analysis/AnalysisCache.h"

namespace audium {

ProjectMonitor::ProjectMonitor(std::shared_ptr<AudiumEngine> engine_) :
    engine(engine_)
{
    engine->getUndoManager()->addChangeListener(this);
    startTimer(intervalMilliseconds);
}

ProjectMonitor::~ProjectMonitor()
{
    stopTimer();
    engine->getUndoManager()->removeChangeListener(this);
}

void ProjectMonitor::changeListenerCallback(juce::ChangeBroadcaster*)
{
    // the undo manager broadcasts on every performed/undone/redone action
    autosavePending = true;
}

void ProjectMonitor::timerCallback()
{
    if (suspended)
        return;

    // external-change detection only applies once a project file exists;
    // never-saved projects still get the autosave cadence below
    const auto projectFile = engine->getCurrentProjectFile();
    if (projectFile != juce::File()) {

        if (!projectFile.existsAsFile()) {
            changePending = false;
            if (!missingReported) {
                missingReported = true;
                if (onProjectFileMissing != nullptr)
                    onProjectFileMissing();
            }
        }
        else {
            missingReported = false;

            if (engine->projectChangedOnDisk() || engine->analysisChangedOnDisk()) {
                const auto projectMtime = projectFile.getLastModificationTime();
                const auto analysisMtime = AudiumEngine::projectDirectory
                                               .getChildFile(AnalysisCache::fileName)
                                               .getLastModificationTime();

                if (changePending &&
                    projectMtime == pendingProjectMtime &&
                    analysisMtime == pendingAnalysisMtime) {
                    changePending = false;
                    if (onExternalChange != nullptr)
                        onExternalChange();
                }
                else {
                    // first sighting, or the writer is still mid-sequence - wait
                    // for the mtimes to hold still for one tick
                    changePending = true;
                    pendingProjectMtime = projectMtime;
                    pendingAnalysisMtime = analysisMtime;
                }
                return;
            }
            changePending = false;
        }
    }

    if (autosavePending) {
        autosavePending = false;

        if (engine->getUndoManager()->canUndo()) {
            if (onAutosaveDue != nullptr)
                onAutosaveDue();
        }
        else {
            // the edits were undone (or the history cleared) - the in-memory
            // state matches the last save again, so drop the stale snapshot
            engine->deleteAutosave();
        }
    }
}

} // namespace audium
