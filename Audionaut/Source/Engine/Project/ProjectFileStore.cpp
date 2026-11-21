//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "ProjectFileStore.h"
#include "ProjectSerializer.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Analysis/AnalysisProvider.h"
#include "Engine/Analysis/AnalysisCache.h"
#include "Engine/Undo/UndoableReloadAction.h"
#include "Engine/Project/AgentEditBatch.h"

#include <set>

#if !JUCE_WINDOWS
 #include <unistd.h>
 #include <signal.h>
 #include <cerrno>
#endif

namespace audium {

const char* ProjectFileStore::projectFileExtension = ".audium";
const char* ProjectFileStore::projectFileName = "Project.json";
const char* ProjectFileStore::autosaveFileName = "Autosave.json";
const char* ProjectFileStore::autosavePidFileName = "Autosave.pid";


static int getCurrentProcessId()
{
#if JUCE_WINDOWS
    return (int) GetCurrentProcessId();
#else
    return (int) getpid();
#endif
}

bool ProjectFileStore::isProcessAlive (int pid)
{
    if (pid <= 0)
        return false;
#if JUCE_WINDOWS
    if (auto* handle = OpenProcess (PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD) pid)) {
        DWORD exitCode = 0;
        const auto running = GetExitCodeProcess (handle, &exitCode) && exitCode == STILL_ACTIVE;
        CloseHandle (handle);
        return running;
    }
    return false;
#else
    return ::kill ((pid_t) pid, 0) == 0 || errno == EPERM;
#endif
}

juce::File ProjectFileStore::getSerializationBaseDirectory() const
{
    return projectDirectories->getSerializationBaseDirectory();
}

juce::File ProjectFileStore::getAutosaveDirectory() const
{
    return getSerializationBaseDirectory();
}

bool ProjectFileStore::isJsonProjectFile (const juce::File& file)
{
    return (file.existsAsFile() &&
            (file.hasFileExtension(projectFileExtension) ||
             file.hasFileExtension(".json")));
}

bool ProjectFileStore::isValidProjectStructure (const juce::File& file)
{
    return (file.isDirectory() &&
            file.getFileName().endsWith(projectFileExtension) &&
            file.getChildFile(projectFileName).existsAsFile());
}

json ProjectFileStore::readProjectJson (const juce::File& file)
{
    juce::FileInputStream inputStream (file);
    if (!inputStream.openedOk())
        throw std::runtime_error(inputStream.getStatus().getErrorMessage().toStdString());

    auto inputString = Streamable::readJsonText(inputStream);
    if (inputString.empty())
        throw std::runtime_error("empty project file");

    return json::parse(inputString);
}

bool ProjectFileStore::writeJsonAtomically (const juce::File& target, const json& content, std::string& error)
{
    juce::TemporaryFile temp (target);
    auto success = false;
    if (auto out = std::unique_ptr<juce::FileOutputStream>(temp.getFile().createOutputStream())) {
        if (out->failedToOpen()) {
            error = out->getStatus().getErrorMessage().toStdString();
            return false;
        }

        // plain JSON text, see Streamable::writeJsonText
        if (! Streamable::writeJsonText(*out, content)) {
            error = out->getStatus().getErrorMessage().toStdString();
            return false;
        }
        out->flush();
        success = true;
    }

    if (!success) {
        error = "could not create " + temp.getFile().getFullPathName().toStdString();
        return false;
    }

    if (!temp.overwriteTargetFileWithTemporary()) {
        error = "could not replace " + target.getFullPathName().toStdString();
        return false;
    }

    return true;
}

// ==== session orchestration =================================================

ProjectFileStore::ProjectFileStore(std::shared_ptr<AudioTrackContainer> audioTrackContainer_,
                                   std::shared_ptr<AudioResourceContainer> audioResourceContainer_,
                                   std::shared_ptr<PlayListScheduler> playListScheduler_,
                                   std::shared_ptr<juce::UndoManager> undoManager_,
                                   std::shared_ptr<ProjectDirectories> projectDirectories_) :
    audioTrackContainer(audioTrackContainer_),
    audioResourceContainer(audioResourceContainer_),
    playListScheduler(playListScheduler_),
    undoManager(undoManager_),
    projectDirectories(projectDirectories_)
{
    jassert(projectDirectories != nullptr);
}

void ProjectFileStore::setSerializer (std::shared_ptr<ProjectSerializer> serializer_)
{
    serializer = serializer_;
}

juce::File ProjectFileStore::autosaveFileFor (const juce::File& projectFile)
{
    return projectFile.getSiblingFile (autosaveFileName);
}

bool ProjectFileStore::reloadWouldLoseEdits (const juce::File& projectFile, bool hasUnsavedEdits)
{
    if (! hasUnsavedEdits)
        return false;

    const auto autosave = autosaveFileFor (projectFile);

    // No snapshot: the edits were never written anywhere, so nothing dates
    // them against the incoming file.
    return autosave.existsAsFile() && projectFile.existsAsFile()
           && autosave.getLastModificationTime() > projectFile.getLastModificationTime();
}

bool ProjectFileStore::open (juce::File inFile, std::function<void (std::string)> callback)
{
    jassert(serializer != nullptr);

    try
    {
        if (inFile == juce::File()) {
            // empty file means: user canceled -> do nothing
            return true;
        }
        else if (audioResourceContainer->getAudioFormatManager()->findFormatForFileExtension(inFile.getFileExtension())) {
            // try to open an audio file
            audioTrackContainer->addAudioFiles({inFile.getFullPathName()},
                                               0.0,
                                               callback,
                                               true);
            return true;
        }
        else {

            if (isValidProjectStructure(inFile))
                inFile = inFile.getChildFile(projectFileName);

            if (inFile.existsAsFile()) {
                std::cout << "loading: " << inFile.getFullPathName() << std::endl;
                audioResourceContainer->createTemporaryProjectDirectory(true);

                projectDirectories->setProjectDirectory(inFile.getParentDirectory());

                // Load persisted analysis data before reading the project: the
                // subsequent rebuild clears tracks but not the analysis cache,
                // so segments are available as soon as the UI queries them.
                audioTrackContainer->getAnalysisProvider()->getCache()->loadFromFolder(getProjectDirectory());

                auto projectJson = readProjectJson(inFile);

                if (serializer->readFromJson(projectJson, true)) {
                    currentProjectFile = inFile;
                    undoManager->clearUndoHistory();
                    currentAgentBatch.reset();
                    playListScheduler->commitPlayListData();
                    refreshDiskStamps();
                    changedExternally = false;
                    viewChanged = false;

                    auto audioDir = AudioResourceContainer::getAudioFileDirectory(getProjectDirectory());
                    audioResourceContainer->deleteObsoleteAudioFiles(audioDir);
                    return true;
                }
            }
            else {
                NullCheckedInvocation::invoke (callback, "file not found");
                return false;

            }

        }


        // we failed to read :(
        NullCheckedInvocation::invoke (callback, "unknown error");
    }
    catch (std::exception &ex)
    {
        std::cout << ex.what() << std::endl;
        NullCheckedInvocation::invoke (callback, ex.what());
    }

    serializer->cleanup();
    serializer->createNewProject();

    return false;
}

std::string ProjectFileStore::noWriteAccessMessage()
{
    std::string message = "No write access. Please select a different location.";
#if JUCE_MAC
    message += "\n\n";
    message += "As a 'Sandboxed App' you are only allowed to save files in the Music folder.";
#endif
    return message;
}

bool ProjectFileStore::save (const juce::File& file_, std::function<void (std::string)> callback)
{
    try
    {
        auto file = file_;

        if (!juce::File(file).hasWriteAccess()) {
            NullCheckedInvocation::invoke (callback, noWriteAccessMessage());
            return false;
        }

        // remember where this project was saved before - a Save As must also
        // clear that package's crash-recovery snapshot
        const auto previousProjectDirectory = getProjectDirectory();

        // The package is only complete once Project.json is in place, so
        // everything done before that point must be reversible: the audio is
        // copied (never moved) and the session re-pointed at the copies, and
        // if the JSON can't be written the copies, any directory created here
        // and the re-pointing are undone again. That way a failed save never
        // leaves a half-created package behind, and the package (or temp
        // directory) it was saved-as from keeps its audio.
        const auto packageDirectory = file.getParentDirectory();
        const auto createdPackage = !packageDirectory.exists();
        if (!packageDirectory.isDirectory())
            packageDirectory.createDirectory(); // Project.json itself arrives by the atomic write's rename

        auto sourceDirectory = AudioResourceContainer::getAudioFileDirectory(getProjectDirectory());
        if (!sourceDirectory.exists()) {
            sourceDirectory = AudioResourceContainer::getAudioFileDirectory(getTemporaryDirectory());
            jassert(sourceDirectory.exists());
        }
        const auto destinationDirectory = AudioResourceContainer::getAudioFileDirectory(packageDirectory);
        const auto relocating = sourceDirectory != destinationDirectory;
        const auto createdAudioDirectory = relocating && !destinationDirectory.exists();

        juce::Array<juce::File> copiedFiles;
        auto relocated = false;

        juce::ErasedScopeGuard discardPartialSave ([&]
        {
            if (relocated) {
                audioResourceContainer->changeAudioFilePaths(sourceDirectory);
                audioTrackContainer->getAnalysisProvider()->getCache()->rebaseAudioFolder(sourceDirectory);
            }
            projectDirectories->setProjectDirectory(previousProjectDirectory);

            for (auto& copied : copiedFiles)
                copied.deleteFile();

            if (createdPackage) {
                packageDirectory.deleteRecursively();
            }
            else if (createdAudioDirectory) {
                // Media/Audio and Media - deleteFile only removes them empty
                destinationDirectory.deleteFile();
                destinationDirectory.getParentDirectory().deleteFile();
            }
        });

        if (relocating) {
            if (!audioResourceContainer->copyAudioFiles(sourceDirectory, destinationDirectory, copiedFiles)) {
                NullCheckedInvocation::invoke (callback, "Failed to copy audio files.");
                return false;
            }

            audioResourceContainer->changeAudioFilePaths(destinationDirectory);

            // Audio files were relocated into the project - re-point the
            // analysis cache at their new location so persisted results survive
            // a Save-As from a temporary/other directory.
            audioTrackContainer->getAnalysisProvider()->getCache()->rebaseAudioFolder(destinationDirectory);
            relocated = true;
        }
        // assign new project directory
        projectDirectories->setProjectDirectory(packageDirectory);

        std::string writeError;
        if (writeJsonToFile(file, writeError)) {
            discardPartialSave.release();

            currentProjectFile = file;
            undoManager->clearUndoHistory();
            currentAgentBatch.reset();

            // Persist analysis results alongside the project file.
            audioTrackContainer->getAnalysisProvider()->getCache()->saveToFolder(getProjectDirectory());

            // A successful save supersedes any crash-recovery snapshot,
            // including a temp-directory one from before the first save and
            // the one in the package this project was saved-as from.
            deleteAutosave();
            if (previousProjectDirectory != getProjectDirectory())
                deleteAutosaveIn(previousProjectDirectory);

            // A first save used to move the audio out of the session's temp
            // directory; the copies are the project's audio now, so the
            // originals there are obsolete.
            if (sourceDirectory == AudioResourceContainer::getAudioFileDirectory(getTemporaryDirectory()))
                for (auto& copied : copiedFiles)
                    sourceDirectory.getChildFile(copied.getFileName()).deleteFile();

            refreshDiskStamps();

            // the on-disk file now reflects this session's state
            changedExternally = false;
            viewChanged = false;
            return true;
        }

        jassert(!writeError.empty());
        NullCheckedInvocation::invoke (callback, writeError.empty() ? "unknown error" : writeError);
    }
    catch (std::exception &ex)
    {
        std::cout << ex.what() << std::endl;
        NullCheckedInvocation::invoke (callback, ex.what());
    }
    return false;
}

bool ProjectFileStore::writeAutosave()
{
    const auto target = getAutosaveDirectory();
    if (target == juce::File() || !target.isDirectory())
        return false;

    std::string error;
    if (!writeJsonToFile(target.getChildFile(autosaveFileName), error))
        return false;

    writePidGuardIfTemporary(target);
    return true;
}

void ProjectFileStore::deleteAutosave()
{
    deleteAutosaveIn(getProjectDirectory());
    deleteAutosaveIn(getTemporaryDirectory());
}

bool ProjectFileStore::restoreAutosave (std::function<void (std::string)> callback)
{
    return applyFileAsUndoableReload(getProjectDirectory().getChildFile(autosaveFileName), false, false,
                                     "Restore autosave", callback);
}

bool ProjectFileStore::reloadFromDisk (std::function<void (std::string)> callback)
{
    if (currentProjectFile == juce::File()) {
        NullCheckedInvocation::invoke (callback, "no project file open");
        return false;
    }

    // Before the stamps are touched: a render is walking the graph from its
    // worker thread, so the change stays pending on disk and is reloaded by
    // the poll that follows the render, not dropped as seen.
    if (playListScheduler->isOfflineRendering()) {
        NullCheckedInvocation::invoke (callback, "a render is in progress");
        return false;
    }

    // Capture the mtimes BEFORE reading: a write landing during the (possibly
    // long) apply must be re-detected on the next poll, not stamped as seen.
    const auto projectMtimeAtRead = currentProjectFile.getLastModificationTime();
    const auto analysisMtimeAtRead = getProjectDirectory().getChildFile(AnalysisCache::fileName)
                                                     .getLastModificationTime();

    // The analysis cache is content-keyed derived data and stays outside the
    // undo snapshot.
    if (analysisChangedOnDisk())
        audioTrackContainer->getAnalysisProvider()->getCache()->loadFromFolder(getProjectDirectory());

    const auto applied = applyFileAsUndoableReload(currentProjectFile, true, true,
                                                   "Reload changes from disk", callback);

    // Stamp even on failure so an unreadable external write doesn't retrigger
    // the change detector every poll tick.
    setStamps(projectMtimeAtRead, analysisMtimeAtRead);
    return applied;
}

void ProjectFileStore::reloadAnalysisFromDisk()
{
    audioTrackContainer->getAnalysisProvider()->getCache()->loadFromFolder(getProjectDirectory());

    // only the analysis stamp: a project write racing in stays detectable
    setAnalysisStamp(getProjectDirectory().getChildFile(AnalysisCache::fileName)
                                     .getLastModificationTime());
}

void ProjectFileStore::refreshDiskStamps()
{
    refreshStamps(currentProjectFile,
                  getProjectDirectory().getChildFile(AnalysisCache::fileName));
}

bool ProjectFileStore::projectChangedOnDisk() const
{
    if (currentProjectFile == juce::File())
        return false;
    return projectChangedOnDisk(currentProjectFile);
}

bool ProjectFileStore::analysisChangedOnDisk() const
{
    if (currentProjectFile == juce::File())
        return false;
    return analysisChangedOnDisk(getProjectDirectory().getChildFile(AnalysisCache::fileName));
}

std::vector<juce::File> ProjectFileStore::findObsoleteAudioFiles() const
{
    // The package's audio belongs to the project as saved on disk, so the
    // saved Project.json decides what is obsolete - not the in-memory state.
    // Agent edits, autosave restore and their undo/redo apply unsaved states,
    // and a discard compared against those offered audio the saved project
    // still references for the trash.
    if (!currentProjectFile.existsAsFile())
        return {};

    try
    {
        return audioResourceContainer->findObsoleteAudioFiles(readProjectJson(currentProjectFile));
    }
    catch (std::exception &ex)
    {
        // unreadable: nothing can be proven obsolete, so nothing is trashed
        std::cout << "findObsoleteAudioFiles: " << ex.what() << std::endl;
    }
    return {};
}

void ProjectFileStore::deleteObsoleteAudioFiles()
{
    audioResourceContainer->trashRedundantFiles(findObsoleteAudioFiles());
}

void ProjectFileStore::markViewChanged()
{
    if (viewChanged)
        return;
    viewChanged = true;
    undoManager->sendChangeMessage();   // what there is to save changed, as with an edit
}

bool ProjectFileStore::hasUnsavedChanges() const
{
    return viewChanged || undoManager->canUndo();
}

void ProjectFileStore::closeProject()
{
    currentProjectFile = juce::File();
    changedExternally = false;
    viewChanged = false;
    currentAgentBatch.reset();
}

bool ProjectFileStore::writeJsonToFile (const juce::File& target, std::string& error)
{
    jassert(serializer != nullptr);

    json serialized;
    if (!serializer->writeToJson(serialized)) {
        error = "failed to serialize the project";
        return false;
    }

    return writeJsonAtomically(target, serialized, error);
}

bool ProjectFileStore::applyFileAsUndoableReload (const juce::File& sourceFile,
                                                  bool preserveUiState,
                                                  bool marksExternalChange,
                                                  const juce::String& transactionName,
                                                  std::function<void (std::string)> callback)
{
    jassert(serializer != nullptr);

    try
    {
        if (!sourceFile.existsAsFile()) {
            NullCheckedInvocation::invoke (callback, "file missing on disk");
            return false;
        }

        return applyStateAsUndoableReload(readProjectJson(sourceFile), preserveUiState,
                                          marksExternalChange, transactionName, callback);
    }
    catch (std::exception &ex)
    {
        std::cout << ex.what() << std::endl;
        NullCheckedInvocation::invoke (callback, ex.what());
    }
    return false;
}

bool ProjectFileStore::applyStateAsUndoableReload (json afterState,
                                                   bool preserveUiState,
                                                   bool marksExternalChange,
                                                   const juce::String& transactionName,
                                                   std::function<void (std::string)> callback)
{
    jassert(serializer != nullptr);

    // The callers (ProjectMonitor, AgentHost) hold off while an export or
    // stem separation renders on a worker thread; this is the backstop for
    // every other route to a rebuild.
    if (playListScheduler->isOfflineRendering()) {
        NullCheckedInvocation::invoke (callback, "a render is in progress");
        return false;
    }

    try
    {
        json beforeState;
        serializer->writeToJson(beforeState);

        // The user's current view wins over whatever the writer stored -
        // an applied state must not move their scroll/zoom.
        if (preserveUiState &&
            beforeState.contains("audium") && beforeState["audium"].contains("ui_state"))
            afterState["audium"]["ui_state"] = beforeState["audium"]["ui_state"];

        const auto performed = undoManager->perform(new UndoableReloadAction(*serializer, *this, beforeState, afterState,
                                                                             preserveUiState, marksExternalChange),
                                                    transactionName);
        if (!performed) {
            NullCheckedInvocation::invoke (callback, "failed to apply the project state");
            return false;
        }

        undoManager->beginNewTransaction();
        return true;
    }
    catch (std::exception &ex)
    {
        std::cout << ex.what() << std::endl;
        NullCheckedInvocation::invoke (callback, ex.what());
    }
    return false;
}

static void eraseSelection (json& node)
{
    if (node.is_object()) {
        node.erase ("selected");
        for (auto& [key, value] : node.items()) eraseSelection (value);
    }
    else if (node.is_array()) {
        for (auto& value : node) eraseSelection (value);
    }
}

/** Erases where the insert editors' windows are and whether they are open
    from an insert chain's array, if `owner` has one under `key`. */
static void eraseEditorWindows (json& owner, const char* key)
{
    if (! owner.is_object())
        return;
    auto inserts = owner.find (key);
    if (inserts == owner.end() || ! inserts->is_array())
        return;
    for (auto& insert : *inserts)
        if (insert.is_object())
            insert.erase ("editor");
}

json ProjectFileStore::documentOf (json state)
{
    if (auto audium = state.find ("audium"); audium != state.end() && audium->is_object()) {
        audium->erase ("scheduler");
        audium->erase ("ui_state");
        eraseSelection (*audium); // clip selection: a click, not an edit

        // an insert editor's window moved, opened or closed: not an edit either
        eraseEditorWindows (*audium, "master_inserts");
        for (const auto* owners : { "audio_tracks", "aux_buses" })
            if (auto list = audium->find (owners); list != audium->end() && list->is_array())
                for (auto& owner : *list)
                    eraseEditorWindows (owner, "inserts");
    }
    return state;
}

/** Calls `f (clip)` for every play list item of every track in `state`
    (`State` is `json` or `const json`). */
template <typename State, typename Function>
static void forEachClip (State& state, Function&& f)
{
    auto audium = state.find ("audium");
    if (audium == state.end() || ! audium->is_object())
        return;

    auto tracks = audium->find ("audio_tracks");
    if (tracks == audium->end() || ! tracks->is_array())
        return;

    for (auto& track : *tracks) {
        if (! track.is_object())
            continue;

        auto clips = track.find ("play_list_vector");
        if (clips == track.end() || ! clips->is_array())
            continue;

        for (auto& clip : *clips)
            if (clip.is_object())
                f (clip);
    }
}

/** What tells a clip apart across two states of one document. */
static std::string clipKey (const json& clip)
{
    return clip.value ("track_id", json()).dump() + "/"
         + clip.value ("region_id", json()).dump() + "/"
         + clip.value ("position_clocks", json()).dump();
}

/** Puts `live`'s clip selection on `state`: a click while a verb ran is the
    user's, and the verb's engine never saw it. A clip the verb created is
    unselected. */
static void carrySelection (const json& live, json& state)
{
    std::set<std::string> selected;
    forEachClip (live, [&] (const json& clip) {
        if (clip.value ("selected", false))
            selected.insert (clipKey (clip));
    });

    forEachClip (state, [&] (json& clip) {
        clip["selected"] = selected.count (clipKey (clip)) > 0;
    });
}

bool ProjectFileStore::canExtendAgentBatch (const AgentEditBatch& batch,
                                            const AgentEditGroup& group,
                                            const json& liveDocument,
                                            juce::Time now) const
{
    // a bare CLI call carries no session: every verb is a step of its own
    if (group.session.isEmpty() || batch.key != group.key())
        return false;

    // ours has to be the last step still, and on the document: an undo of it,
    // or any other edit's step since, ends the batch (a redo of ours brings
    // it back). The flag tells our step from another one named like it.
    if (! batch.applied || undoManager->getUndoDescription() != batch.transactionName)
        return false;

    // extending adds nothing to the history, so it would leave a redo the
    // user still has - one that then overwrites the verb. A step of its own
    // drops that redo, as any edit does.
    if (undoManager->canRedo())
        return false;

    // a user edit between two verbs leaves a document that is not the one
    // the batch left - the same check the host makes before applying at all
    if (liveDocument != batch.afterDocument)
        return false;

    return now - batch.lastEdit <= juce::RelativeTime::minutes (agentBatchIdleMinutes);
}

bool ProjectFileStore::applyAgentState (json afterState,
                                        const AgentEditGroup& group,
                                        std::function<void (std::string)> callback)
{
    jassert(serializer != nullptr);

    json liveState;
    try {
        serializer->writeToJson (liveState);
    }
    catch (std::exception& ex) {
        std::cout << ex.what() << std::endl;
        NullCheckedInvocation::invoke (callback, ex.what());
        return false;
    }

    return applyAgentState (std::move (afterState), std::move (liveState), group, std::move (callback));
}

bool ProjectFileStore::applyAgentState (json afterState,
                                        json liveState,
                                        const AgentEditGroup& group,
                                        std::function<void (std::string)> callback)
{
    jassert(serializer != nullptr);

    if (playListScheduler->isOfflineRendering()) {
        NullCheckedInvocation::invoke (callback, "a render is in progress");
        return false;
    }

    try
    {
        // the user's current view and selection win over whatever the
        // verb's engine stored
        if (liveState.contains ("audium") && liveState["audium"].contains ("ui_state"))
            afterState["audium"]["ui_state"] = liveState["audium"]["ui_state"];
        carrySelection (liveState, afterState);

        const auto now = clock();
        const auto liveDocument = documentOf (liveState);

        if (auto batch = currentAgentBatch.lock(); batch != nullptr && canExtendAgentBatch (*batch, group, liveDocument, now)) {
            auto applied = false;
            try {
                applied = serializer->applyProjectJson (afterState, true);
            }
            catch (std::exception& ex) {
                std::cout << ex.what() << std::endl;
            }

            if (! applied) {
                // best effort: back to where the batch left the document
                try { serializer->applyProjectJson (batch->after, true); } catch (...) {}
                NullCheckedInvocation::invoke (callback, "failed to apply the project state");
                return false;
            }

            batch->setAfter (std::move (afterState));
            ++batch->numEdits;
            batch->lastEdit = now;
            setChangedExternally (true);

            // nothing was performed, but the document changed as if it had:
            // the listeners (title, menus, autosave) hear about it the same way
            undoManager->sendChangeMessage();
            return true;
        }

        auto batch = std::make_shared<AgentEditBatch>();
        batch->key = group.key();
        batch->transactionName = group.task.isNotEmpty() ? "Agent: " + group.task : juce::String ("Agent edits");
        batch->before = std::move (liveState);
        batch->setAfter (std::move (afterState));
        batch->numEdits = 1;
        batch->lastEdit = now;

        // the action owns the batch from here; we keep watching it
        const auto performed = undoManager->perform (new UndoableAgentBatchAction (*serializer, *this, batch),
                                                     batch->transactionName);
        if (! performed) {
            NullCheckedInvocation::invoke (callback, "failed to apply the project state");
            return false;
        }

        undoManager->beginNewTransaction();
        // nothing to extend without a session; the next verb starts its own step
        if (group.session.isNotEmpty())
            currentAgentBatch = batch;
        else
            currentAgentBatch.reset();
        return true;
    }
    catch (std::exception &ex)
    {
        std::cout << ex.what() << std::endl;
        NullCheckedInvocation::invoke (callback, ex.what());
    }
    return false;
}

// ==== file primitives =======================================================

void ProjectFileStore::refreshStamps (const juce::File& projectFile, const juce::File& analysisFile)
{
    projectFileStamp = projectFile.getLastModificationTime();
    analysisFileStamp = analysisFile.getLastModificationTime();
}

void ProjectFileStore::setStamps (juce::Time projectMtime, juce::Time analysisMtime)
{
    projectFileStamp = projectMtime;
    analysisFileStamp = analysisMtime;
}

void ProjectFileStore::setAnalysisStamp (juce::Time analysisMtime)
{
    analysisFileStamp = analysisMtime;
}

bool ProjectFileStore::projectChangedOnDisk (const juce::File& projectFile) const
{
    return projectFile.getLastModificationTime() != projectFileStamp;
}

bool ProjectFileStore::analysisChangedOnDisk (const juce::File& analysisFile) const
{
    return analysisFile.getLastModificationTime() != analysisFileStamp;
}

void ProjectFileStore::writePidGuardIfTemporary (const juce::File& packageDirectory)
{
    const auto tempRoot = juce::File::getSpecialLocation(juce::File::tempDirectory);
    if (packageDirectory == tempRoot || packageDirectory.isAChildOf(tempRoot))
        packageDirectory.getChildFile(autosavePidFileName)
                        .replaceWithText(juce::String(getCurrentProcessId()));
}

void ProjectFileStore::deleteAutosaveIn (const juce::File& packageDirectory)
{
    if (packageDirectory != juce::File()) {
        packageDirectory.getChildFile(autosaveFileName).deleteFile();
        packageDirectory.getChildFile(autosavePidFileName).deleteFile();
    }
}

juce::File ProjectFileStore::findOrphanedTempAutosave() const
{
    return findOrphanedTempAutosave(getTemporaryDirectory());
}

juce::File ProjectFileStore::findOrphanedTempAutosave (const juce::File& currentTempDirectory)
{
    juce::File newest;
    juce::Time newestTime;

    const auto tempRoot = juce::File::getSpecialLocation(juce::File::tempDirectory);
    const auto pattern = "temp-*" + juce::String(projectFileExtension);

    for (const auto& dir : tempRoot.findChildFiles(juce::File::findDirectories, false, pattern)) {

        if (dir == currentTempDirectory)
            continue; // this session's own temp directory

        const auto autosave = dir.getChildFile(autosaveFileName);
        if (!autosave.existsAsFile())
            continue;

        // a directory whose Project.json is current was already restored
        const auto projectJson = dir.getChildFile(projectFileName);
        if (projectJson.existsAsFile() &&
            projectJson.getLastModificationTime() >= autosave.getLastModificationTime())
            continue;

        // skip snapshots owned by another live instance
        const auto pidFile = dir.getChildFile(autosavePidFileName);
        if (pidFile.existsAsFile() && isProcessAlive(pidFile.loadFileAsString().getIntValue()))
            continue;

        if (newest == juce::File() || autosave.getLastModificationTime() > newestTime) {
            newestTime = autosave.getLastModificationTime();
            newest = dir;
        }
    }

    return newest;
}

} // namespace audium
