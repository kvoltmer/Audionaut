//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "ProjectMonitor.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Engine/Analysis/AnalysisCache.h"

namespace audium {

ProjectMonitor::ProjectMonitor(std::shared_ptr<AudiumEngine> engine_) :
    engine(engine_),
    store(engine_->getProjectFileStore())
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
    const auto projectFile = store->getCurrentProjectFile();
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

            if (store->projectChangedOnDisk() || store->analysisChangedOnDisk()) {
                const auto projectMtime = projectFile.getLastModificationTime();
                const auto analysisMtime = ProjectFileStore::projectDirectory
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
        else if (engine->getUndoManager()->canRedo()) {
            // the edits were undone back to the saved state, so the snapshot
            // is stale. Only the undone-to-clean case (canRedo) may delete:
            // clearUndoHistory (open/save) also broadcasts, and deleting then
            // would destroy a freshly opened package's restorable snapshot.
            store->deleteAutosave();
        }
    }
}

} // namespace audium
