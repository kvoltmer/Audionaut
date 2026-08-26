//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "AudiumEngine.h"
#include "Util/Preferences.h"
#include "Engine/ActionMessages.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/Link/LinkAudioDevice.h"
#include "Engine/Factory/AudioTrackFactory.h"
#include "Engine/Analysis/AnalysisProvider.h"
#include "Engine/Analysis/AnalysisCache.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/AudioSources/VoiceSource.h"
#include "Engine/Undo/UndoableReloadAction.h"
#include "Application/AudiumApplication.h"

#include "Interface/ColourIds.h"

#if !JUCE_WINDOWS
 #include <unistd.h>
 #include <signal.h>
 #include <cerrno>
#endif

namespace audium {

const char* AudiumEngine::projectFileExtension = ".audium";
const char* AudiumEngine::projectFileName = "Project.json";
const char* AudiumEngine::autosaveFileName = "Autosave.json";
const char* AudiumEngine::autosavePidFileName = "Autosave.pid";

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
juce::File AudiumEngine::projectDirectory = File();
juce::File AudiumEngine::tempDirectory = File();
int AudiumEngine::recordingCounter = 0;

AudiumEngine::~AudiumEngine()
{
    cleanup();
}

void AudiumEngine::initialise()
{
    jassert(RuntimePermissions::isGranted (RuntimePermissions::recordAudio));
    
    auto numInputChannelsNeeded = MAX_AUDIO_CHANNELS;
    auto numOutputChannelsNeeded = MAX_AUDIO_CHANNELS;
    String result;

#if !defined(AUDIONAUT_HEADLESS)
    if (AudiumApplication::getPreferences().valueExists(PreferenceKeys::audioDeviceSettings)) {
        juce::XmlDocument xml (AudiumApplication::getPreferences().getValue(PreferenceKeys::audioDeviceSettings));
        if (auto saveState = xml.getDocumentElement()) {
            result = audioDeviceManager->initialise(numInputChannelsNeeded,
                                                    numOutputChannelsNeeded,
                                                    saveState.get(),
                                                    true);
        }
    }
    else {
        result = audioDeviceManager->initialiseWithDefaultDevices (numInputChannelsNeeded,
                                                                   numOutputChannelsNeeded);
    }
#else
    result = audioDeviceManager->initialiseWithDefaultDevices (numInputChannelsNeeded,
                                                               numOutputChannelsNeeded);
#endif
    std::cout << result.toStdString() << std::endl;
    audioDeviceManager->addAudioCallback(linkAudioDevice.get());
}

void AudiumEngine::uninitialise()
{
#if !defined(AUDIONAUT_HEADLESS)
    if (auto stateXml = audioDeviceManager->createStateXml()) {
        AudiumApplication::getPreferences().setValue(PreferenceKeys::audioDeviceSettings, stateXml->toString().toStdString());
    }
#endif
    
    undoManager->clearUndoHistory();
    audioDeviceManager->removeAudioCallback(linkAudioDevice.get());

    // a clean shutdown passed the save/discard prompt - anything left in an
    // autosave would wrongly look like a crash on the next launch
    deleteAutosave();
}

void AudiumEngine::cleanup()
{
    audioTrackContainer->cleanup();
    audioResourceContainer->cleanup();
    undoManager->clearUndoHistory();
    
    currentProjectFile = File();
    currentJson.clear();
    uiState.clear();
    changedExternally = false;
}

void AudiumEngine::createNewProject(const int numChannels)
{
    // reset current project dir
    projectDirectory = File();

    // A fresh project starts with no analysis data.
    audioTrackContainer->getAnalysisProvider()->getCache()->clear();

    AudioResourceContainer::createTemporaryProjectDirectory(true);
    
    audium::WaveFormColours::resetWaveFormColour();
    for (auto i = 0; i < 1; i++) {
        auto track = audioTrackContainer->createNewAudioTrack("Track " + String(i+1));
        track->getViewState().setColour(audioTrackContainer->getNewAudioTrackColour());
        for (auto c = 0; c < numChannels; c++) {
            track->addChannel();
        }
    }
}

bool AudiumEngine::isJsonProjectFile(const juce::File &file)
{
    // returns true for an explicit project file:
    // foo.json
    // or legacy -> foo.audium
    
    return (file.existsAsFile() &&
            (file.hasFileExtension(projectFileExtension) ||
             file.hasFileExtension(".json")));
}

bool AudiumEngine::isValidProjectStructure(const juce::File &file)
{
    // expected structure for foo is:
    // foo.audium/
    // foo.audium/Project.json
    
    return (file.isDirectory() &&
            file.getFileName().endsWith(projectFileExtension) &&
            File(file.getFullPathName() + File::getSeparatorString() + projectFileName).existsAsFile());
}

bool AudiumEngine::openFile (juce::File inFile, std::function<void (std::string)> callback)
{
    try
    {
        if (inFile == File()) {
            // empty file means: user canceled -> do nothing
            return true;
        }
        else if (getAudioResourceContainer()->getAudioFormatManager()->findFormatForFileExtension(inFile.getFileExtension())) {
            // try to open an audio file
            getAudioTrackContainer()->addAudioFiles({inFile.getFullPathName()},
                                                    0.0,
                                                    callback,
                                                    true);
            return true;
        }
        else {
        
            if (isValidProjectStructure(inFile))
                inFile = File(inFile.getFullPathName() + File::getSeparatorString() + projectFileName);
            
            juce::FileInputStream inputStream(inFile);
            if (inputStream.openedOk()) {
                std::cout << "loading: " << inFile.getFullPathName() << std::endl;
                AudioResourceContainer::createTemporaryProjectDirectory(true);
                
                projectDirectory = inFile.getParentDirectory();

                // Load persisted analysis data before reading the project: the
                // subsequent rebuild clears tracks but not the analysis cache,
                // so segments are available as soon as the UI queries them.
                audioTrackContainer->getAnalysisProvider()->getCache()->loadFromFolder(projectDirectory);

                if (readFromStream(inputStream, true)){
                    currentProjectFile = inFile;
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
                NullCheckedInvocation::invoke (callback,
                                               inputStream.getStatus().getErrorMessage().toStdString());
                return false;
                
            }
            
        }

        
        // we failed to read :(
        NullCheckedInvocation::invoke (callback, "unknown error");
    }
    catch (std::exception &e)
    {
        std::cout << e.what() << std::endl;
        NullCheckedInvocation::invoke (callback, e.what());
    }
    
    cleanup();
    createNewProject();
    
    return false;
    
}

bool AudiumEngine::saveFile (const juce::File& file_, std::function<void (std::string)> callback)
{
    try
    {
        auto file = file_;
        
        if (!File(file).hasWriteAccess()) {
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
        if (writeJsonToFile(file, writeError)) {
            currentProjectFile = file;
            undoManager->clearUndoHistory();

            // Persist analysis results alongside the project file.
            audioTrackContainer->getAnalysisProvider()->getCache()->saveToFolder(projectDirectory);

            // A successful save supersedes any crash-recovery snapshot,
            // including a temp-directory one from before the first save.
            deleteAutosave();

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
    catch (std::exception &e)
    {
        std::cout << e.what() << std::endl;
        NullCheckedInvocation::invoke (callback, e.what());
    }
    return false;
}

void AudiumEngine::setBypass(bool bypass)
{
    linkAudioDevice->setBypass(bypass);
}

bool AudiumEngine::writeToStream (juce::OutputStream& outputStream)
{
    return audium::Streamable::writeToStream(outputStream);
}

bool AudiumEngine::readFromStream (juce::InputStream& inputStream, bool rebuild)
{
    setBypass(true);
    auto result = audium::Streamable::readFromStream(inputStream);
    // std::cout << "AudiumEngine::readFromStream done" << std::endl;
    setBypass(false);
    return result;
}

bool AudiumEngine::writeToJson (json& output)
{
    
    json jsonAudium;
    audioTrackContainer->writeToJson(jsonAudium);
    
    jsonAudium["tempo"] = playListScheduler->getTempoProvider()->getTempo();
    jsonAudium["file_version"] = audium::Streamable::fileVersion;
    jsonAudium["ui_state"] = uiState;
    jsonAudium["scheduler"] = getPlayListScheduler()->data;
    output["audium"] = jsonAudium;
    currentJson = output;
    // std::cout << std::setw(2) << output << std::endl;
    return true;
}

bool AudiumEngine::readFromJson (json& input, bool rebuild)
{
    // std::cout << std::setw(2) << input << std::endl;
    auto jsonAudium = input["audium"];
    
    cleanup(); // clear everything
    
    const auto tempo = jsonAudium["tempo"].template get<double>();
    if (jsonAudium.contains("file_version"))
    {
        const auto version = jsonAudium["file_version"].template get<int>();
        jassert(version == audium::Streamable::fileVersion);
    }
    
    if (jsonAudium.contains("ui_state"))
        uiState = jsonAudium["ui_state"];
    
    if (jsonAudium.contains("scheduler"))
        getPlayListScheduler()->data = jsonAudium["scheduler"];
    
    
    if (!linkAudioDevice->getLinkEngine()->isEnabled()) // don't interfere with running sessions
        playListScheduler->getTempoProvider()->setTempo(tempo);
    
    if (audioTrackContainer->readFromJson(jsonAudium, rebuild)) {
        currentJson = input;
        return true;
    }
    return false;
}

bool AudiumEngine::applyProjectJson (json& input, bool preserveUiState)
{
    auto jsonAudium = input["audium"];

    const auto tempo = jsonAudium["tempo"].template get<double>();
    if (jsonAudium.contains("file_version")) {
        const auto version = jsonAudium["file_version"].template get<int>();
        jassert(version == audium::Streamable::fileVersion);
    }

    if (!preserveUiState && jsonAudium.contains("ui_state"))
        uiState = jsonAudium["ui_state"];

    if (jsonAudium.contains("scheduler"))
        getPlayListScheduler()->data = jsonAudium["scheduler"];

    if (!linkAudioDevice->getLinkEngine()->isEnabled()) // don't interfere with running sessions
        playListScheduler->getTempoProvider()->setTempo(tempo);

    // Non-destructive read; AudioTrackContainer falls back to a full rebuild
    // when the structure differs. Bypass the audio callback for the duration.
    setBypass(true);
    const auto readOk = audioTrackContainer->readFromJson(jsonAudium, false);
    setBypass(false);

    if (!readOk)
        return false;

    currentJson = input;
    playListScheduler->commitPlayListData();

    // The UI/scheduler broadcasts normally happen in
    // AudioTrackContainer::readFromStream; applying JSON directly must
    // publish them here.
    audioTrackContainer->sendActionMessage(rebuildAll);
    audioTrackContainer->sendChangeMessage();

    return true;
}

bool AudiumEngine::applyFileAsUndoableReload (const juce::File& sourceFile,
                                              bool preserveUiState,
                                              bool marksExternalChange,
                                              const juce::String& transactionName,
                                              std::function<void (std::string)> callback)
{
    try
    {
        if (!sourceFile.existsAsFile()) {
            NullCheckedInvocation::invoke (callback, "file missing on disk");
            return false;
        }

        juce::FileInputStream inputStream (sourceFile);
        if (!inputStream.openedOk()) {
            NullCheckedInvocation::invoke (callback, inputStream.getStatus().getErrorMessage().toStdString());
            return false;
        }

        // The file is framed by juce::OutputStream::writeString (see
        // Streamable) - read it back the same way before parsing.
        auto inputString = inputStream.readString().toStdString();
        if (inputString.empty())
            throw std::runtime_error("empty project file");
        auto afterState = json::parse(inputString);

        json beforeState;
        writeToJson(beforeState);

        // The user's current view wins over whatever the external writer
        // stored - reloads must not move their scroll/zoom.
        if (preserveUiState &&
            beforeState.contains("audium") && beforeState["audium"].contains("ui_state"))
            afterState["audium"]["ui_state"] = beforeState["audium"]["ui_state"];

        undoManager->perform(new UndoableReloadAction(*this, beforeState, afterState,
                                                      preserveUiState, marksExternalChange),
                             transactionName);
        undoManager->beginNewTransaction();
        return true;
    }
    catch (std::exception &e)
    {
        std::cout << e.what() << std::endl;
        NullCheckedInvocation::invoke (callback, e.what());
    }
    return false;
}

bool AudiumEngine::reloadFromDisk (std::function<void (std::string)> callback)
{
    if (currentProjectFile == File()) {
        NullCheckedInvocation::invoke (callback, "no project file open");
        return false;
    }

    // The analysis cache is content-keyed derived data and stays outside the
    // undo snapshot.
    if (analysisChangedOnDisk())
        audioTrackContainer->getAnalysisProvider()->getCache()->loadFromFolder(projectDirectory);

    const auto applied = applyFileAsUndoableReload(currentProjectFile, true, true,
                                                   "Reload changes from disk", callback);

    // Refresh even on failure so an unreadable external write doesn't retrigger
    // the change detector every poll tick.
    refreshDiskStamps();
    return applied;
}

bool AudiumEngine::restoreAutosave (std::function<void (std::string)> callback)
{
    return applyFileAsUndoableReload(projectDirectory.getChildFile(autosaveFileName), false, false,
                                     "Restore autosave", callback);
}

juce::File AudiumEngine::getSerializationBaseDirectory()
{
    if (projectDirectory != File() && projectDirectory.isDirectory())
        return projectDirectory;
    return tempDirectory;
}

juce::File AudiumEngine::getAutosaveDirectory()
{
    return getSerializationBaseDirectory();
}

bool AudiumEngine::writeAutosave()
{
    const auto target = getAutosaveDirectory();
    if (target == File() || !target.isDirectory())
        return false;

    std::string error;
    if (!writeJsonToFile(target.getChildFile(autosaveFileName), error))
        return false;

    // temp-directory snapshots are found by findOrphanedTempAutosave - the
    // pid file keeps it from claiming a running instance's session
    if (target == tempDirectory)
        target.getChildFile(autosavePidFileName).replaceWithText(String(getCurrentProcessId()));

    return true;
}

void AudiumEngine::deleteAutosave()
{
    for (auto& dir : { projectDirectory, tempDirectory }) {
        if (dir != File()) {
            dir.getChildFile(autosaveFileName).deleteFile();
            dir.getChildFile(autosavePidFileName).deleteFile();
        }
    }
}

juce::File AudiumEngine::findOrphanedTempAutosave()
{
    juce::File newest;
    juce::Time newestTime;

    const auto tempRoot = File::getSpecialLocation(File::tempDirectory);
    const auto pattern = "temp-*" + String(projectFileExtension);

    for (const auto& dir : tempRoot.findChildFiles(File::findDirectories, false, pattern)) {

        if (dir == tempDirectory)
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

        if (newest == File() || autosave.getLastModificationTime() > newestTime) {
            newestTime = autosave.getLastModificationTime();
            newest = dir;
        }
    }

    return newest;
}

bool AudiumEngine::writeJsonToFile (const juce::File& target, std::string& error)
{
    juce::TemporaryFile temp (target);
    auto success = false;
    if (auto out = std::unique_ptr<juce::FileOutputStream>(temp.getFile().createOutputStream())) {
        if (out->failedToOpen()) {
            error = out->getStatus().getErrorMessage().toStdString();
            return false;
        }

        if (writeToStream (*out.get())) {
            out->flush();
            success = true;
        }
    }

    return success && temp.overwriteTargetFileWithTemporary();
}

void AudiumEngine::refreshDiskStamps()
{
    projectFileStamp = currentProjectFile.getLastModificationTime();
    analysisFileStamp = projectDirectory.getChildFile(AnalysisCache::fileName).getLastModificationTime();
}

bool AudiumEngine::projectChangedOnDisk() const
{
    if (currentProjectFile == File())
        return false;
    return currentProjectFile.getLastModificationTime() != projectFileStamp;
}

bool AudiumEngine::analysisChangedOnDisk() const
{
    if (currentProjectFile == File())
        return false;
    return projectDirectory.getChildFile(AnalysisCache::fileName).getLastModificationTime() != analysisFileStamp;
}

int AudiumEngine::getSizeInUnits()
{
    return audioTrackContainer->getSizeInUnits() + 1;
}

std::shared_ptr<PlayListContainer> AudiumEngine::getPlayListContainer(std::shared_ptr<AudioTrack> track) const
{
    return track->getPlayListContainer();
}

void AudiumEngine::deleteObsoleteAudioFiles()
{
    audioResourceContainer->deleteObsoleteAudioFiles(currentJson);
}

} // namespace audium
