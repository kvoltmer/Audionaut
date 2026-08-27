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

juce::File ProjectFileStore::projectDirectory = juce::File();
juce::File ProjectFileStore::tempDirectory = juce::File();

static int getCurrentProcessId()
{
#if JUCE_WINDOWS
    return (int) GetCurrentProcessId();
#else
    return (int) getpid();
#endif
}

static bool isProcessAlive (int pid)
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

juce::File ProjectFileStore::getSerializationBaseDirectory()
{
    if (projectDirectory != juce::File() && projectDirectory.isDirectory())
        return projectDirectory;
    return tempDirectory;
}

juce::File ProjectFileStore::getAutosaveDirectory()
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

    auto inputString = inputStream.readString().toStdString();
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

        // same framing as Streamable::writeToStream
        out->writeString(content.dump(2));
        out->flush();
        success = true;
    }

    return success && temp.overwriteTargetFileWithTemporary();
}

// ==== session orchestration =================================================

ProjectFileStore::ProjectFileStore(std::shared_ptr<AudioTrackContainer> audioTrackContainer_,
                                   std::shared_ptr<AudioResourceContainer> audioResourceContainer_,
                                   std::shared_ptr<PlayListScheduler> playListScheduler_,
                                   std::shared_ptr<juce::UndoManager> undoManager_) :
    audioTrackContainer(audioTrackContainer_),
    audioResourceContainer(audioResourceContainer_),
    playListScheduler(playListScheduler_),
    undoManager(undoManager_)
{
}

void ProjectFileStore::setSerializer (std::shared_ptr<ProjectSerializer> serializer_)
{
    serializer = serializer_;
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
                AudioResourceContainer::createTemporaryProjectDirectory(true);

                projectDirectory = inFile.getParentDirectory();

                // Load persisted analysis data before reading the project: the
                // subsequent rebuild clears tracks but not the analysis cache,
                // so segments are available as soon as the UI queries them.
                audioTrackContainer->getAnalysisProvider()->getCache()->loadFromFolder(projectDirectory);

                auto projectJson = readProjectJson(inFile);

                if (serializer->readFromJson(projectJson, true)) {
                    currentProjectFile = inFile;
                    currentJson = std::move(projectJson);
                    undoManager->clearUndoHistory();
                    playListScheduler->commitPlayListData();
                    refreshDiskStamps();
                    changedExternally = false;

                    auto audioDir = AudioResourceContainer::getAudioFileDirectory(projectDirectory);
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

bool ProjectFileStore::save (const juce::File& file_, std::function<void (std::string)> callback)
{
    try
    {
        auto file = file_;

        if (!juce::File(file).hasWriteAccess()) {
            std::string errorString = "No write access. Please select a different location.";
#if JUCE_MAC
            errorString += "\n\n";
            errorString += "As a 'Sandboxed App' you are only allowed to save files in the Music folder.";
#endif
            NullCheckedInvocation::invoke (callback, errorString);
            return false;
        }

        if (! file.exists())
            file.create();

        // remember where this project was saved before - a Save As must also
        // clear that package's crash-recovery snapshot
        const auto previousProjectDirectory = projectDirectory;

        // need to copy or move audio files?
        auto sourceDirectory = AudioResourceContainer::getAudioFileDirectory(projectDirectory);
        if (!sourceDirectory.exists()) {
            sourceDirectory = AudioResourceContainer::getAudioFileDirectory(tempDirectory);
            jassert(sourceDirectory.exists());
        }
        auto destinationDirectory = AudioResourceContainer::getAudioFileDirectory(file.getParentDirectory());
        if (sourceDirectory != destinationDirectory) {
            if (!audioResourceContainer->copyOrMoveAudioFiles(sourceDirectory, destinationDirectory)) {
                NullCheckedInvocation::invoke (callback, "Failed to copy audio files.");
                return false;
            }

            audioResourceContainer->changeAudioFilePaths(destinationDirectory);

            // Audio files were relocated into the project - re-point the
            // analysis cache at their new location so persisted results survive
            // a Save-As from a temporary/other directory.
            audioTrackContainer->getAnalysisProvider()->getCache()->rebaseAudioFolder(destinationDirectory);
        }
        // assign new project directory
        projectDirectory = file.getParentDirectory();

        std::string writeError;
        json serialized;
        if (writeJsonToFile(file, writeError, &serialized)) {
            currentProjectFile = file;
            currentJson = std::move(serialized);
            undoManager->clearUndoHistory();

            // Persist analysis results alongside the project file.
            audioTrackContainer->getAnalysisProvider()->getCache()->saveToFolder(projectDirectory);

            // A successful save supersedes any crash-recovery snapshot,
            // including a temp-directory one from before the first save and
            // the one in the package this project was saved-as from.
            deleteAutosave();
            if (previousProjectDirectory != projectDirectory)
                deleteAutosaveIn(previousProjectDirectory);

            refreshDiskStamps();

            // the on-disk file now reflects this session's state
            changedExternally = false;
            return true;
        }

        if (!writeError.empty()) {
            NullCheckedInvocation::invoke (callback, writeError);
            return false;
        }

        jassertfalse;
        NullCheckedInvocation::invoke (callback, "unknown error");
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
    deleteAutosaveIn(projectDirectory);
    deleteAutosaveIn(tempDirectory);
}

bool ProjectFileStore::restoreAutosave (std::function<void (std::string)> callback)
{
    return applyFileAsUndoableReload(projectDirectory.getChildFile(autosaveFileName), false, false,
                                     "Restore autosave", callback);
}

bool ProjectFileStore::reloadFromDisk (std::function<void (std::string)> callback)
{
    if (currentProjectFile == juce::File()) {
        NullCheckedInvocation::invoke (callback, "no project file open");
        return false;
    }

    // Capture the mtimes BEFORE reading: a write landing during the (possibly
    // long) apply must be re-detected on the next poll, not stamped as seen.
    const auto projectMtimeAtRead = currentProjectFile.getLastModificationTime();
    const auto analysisMtimeAtRead = projectDirectory.getChildFile(AnalysisCache::fileName)
                                                     .getLastModificationTime();

    // The analysis cache is content-keyed derived data and stays outside the
    // undo snapshot.
    if (analysisChangedOnDisk())
        audioTrackContainer->getAnalysisProvider()->getCache()->loadFromFolder(projectDirectory);

    const auto applied = applyFileAsUndoableReload(currentProjectFile, true, true,
                                                   "Reload changes from disk", callback);

    // Stamp even on failure so an unreadable external write doesn't retrigger
    // the change detector every poll tick.
    setStamps(projectMtimeAtRead, analysisMtimeAtRead);
    return applied;
}

void ProjectFileStore::reloadAnalysisFromDisk()
{
    audioTrackContainer->getAnalysisProvider()->getCache()->loadFromFolder(projectDirectory);

    // only the analysis stamp: a project write racing in stays detectable
    setAnalysisStamp(projectDirectory.getChildFile(AnalysisCache::fileName)
                                     .getLastModificationTime());
}

void ProjectFileStore::refreshDiskStamps()
{
    refreshStamps(currentProjectFile,
                  projectDirectory.getChildFile(AnalysisCache::fileName));
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
    return analysisChangedOnDisk(projectDirectory.getChildFile(AnalysisCache::fileName));
}

void ProjectFileStore::deleteObsoleteAudioFiles()
{
    audioResourceContainer->deleteObsoleteAudioFiles(currentJson);
}

void ProjectFileStore::closeProject()
{
    currentProjectFile = juce::File();
    currentJson.clear();
    changedExternally = false;
}

bool ProjectFileStore::writeJsonToFile (const juce::File& target, std::string& error, json* serializedOut)
{
    jassert(serializer != nullptr);

    json serialized;
    if (!serializer->writeToJson(serialized)) {
        error = "failed to serialize the project";
        return false;
    }

    if (!writeJsonAtomically(target, serialized, error))
        return false;

    if (serializedOut != nullptr)
        *serializedOut = std::move(serialized);
    return true;
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

        auto afterState = readProjectJson(sourceFile);

        json beforeState;
        serializer->writeToJson(beforeState);

        // The user's current view wins over whatever the external writer
        // stored - reloads must not move their scroll/zoom.
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

juce::File ProjectFileStore::findOrphanedTempAutosave()
{
    return findOrphanedTempAutosave(tempDirectory);
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
