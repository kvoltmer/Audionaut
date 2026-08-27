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

#include "Engine/ProjectFileStore.h"
#include "Interface/ColourIds.h"

namespace audium {

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
    return ProjectFileStore::isJsonProjectFile(file);
}

bool AudiumEngine::isValidProjectStructure(const juce::File &file)
{
    return ProjectFileStore::isValidProjectStructure(file);
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
                inFile = inFile.getChildFile(ProjectFileStore::projectFileName);
            
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
                projectFileStore->deleteAutosaveIn(previousProjectDirectory);

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

    // NOTE: deliberately no currentJson update here. currentJson tracks the
    // state whose file references are authoritative for obsolete-file cleanup;
    // snapshots (autosave, undo captures) must not clobber it - only
    // open/save/apply do, explicitly.
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
    // when the structure differs. Bypass the audio callback for the duration -
    // and make sure a throwing read can't leave it bypassed forever.
    setBypass(true);
    auto readOk = false;
    try {
        readOk = audioTrackContainer->readFromJson(jsonAudium, false);
    }
    catch (...) {
        setBypass(false);
        throw;
    }
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

        auto afterState = ProjectFileStore::readProjectJson(sourceFile);

        json beforeState;
        writeToJson(beforeState);

        // The user's current view wins over whatever the external writer
        // stored - reloads must not move their scroll/zoom.
        if (preserveUiState &&
            beforeState.contains("audium") && beforeState["audium"].contains("ui_state"))
            afterState["audium"]["ui_state"] = beforeState["audium"]["ui_state"];

        const auto performed = undoManager->perform(new UndoableReloadAction(*this, beforeState, afterState,
                                                                             preserveUiState, marksExternalChange),
                                                    transactionName);
        if (!performed) {
            NullCheckedInvocation::invoke (callback, "failed to apply the project state");
            return false;
        }

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
    projectFileStore->setStamps(projectMtimeAtRead, analysisMtimeAtRead);
    return applied;
}

void AudiumEngine::reloadAnalysisFromDisk()
{
    audioTrackContainer->getAnalysisProvider()->getCache()->loadFromFolder(projectDirectory);

    // only the analysis stamp: a project write racing in stays detectable
    projectFileStore->setAnalysisStamp(projectDirectory.getChildFile(AnalysisCache::fileName)
                                                       .getLastModificationTime());
}

bool AudiumEngine::restoreAutosave (std::function<void (std::string)> callback)
{
    return applyFileAsUndoableReload(projectDirectory.getChildFile(ProjectFileStore::autosaveFileName), false, false,
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
    if (!writeJsonToFile(target.getChildFile(ProjectFileStore::autosaveFileName), error))
        return false;

    projectFileStore->writePidGuardIfTemporary(target);
    return true;
}

void AudiumEngine::deleteAutosave()
{
    projectFileStore->deleteAutosaveIn(projectDirectory);
    projectFileStore->deleteAutosaveIn(tempDirectory);
}

juce::File AudiumEngine::findOrphanedTempAutosave()
{
    return ProjectFileStore::findOrphanedTempAutosave(tempDirectory);
}

bool AudiumEngine::writeJsonToFile (const juce::File& target, std::string& error, json* serializedOut)
{
    json serialized;
    if (!writeToJson(serialized)) {
        error = "failed to serialize the project";
        return false;
    }

    if (!projectFileStore->writeJsonAtomically(target, serialized, error))
        return false;

    if (serializedOut != nullptr)
        *serializedOut = std::move(serialized);
    return true;
}

void AudiumEngine::refreshDiskStamps()
{
    projectFileStore->refreshStamps(currentProjectFile,
                                    projectDirectory.getChildFile(AnalysisCache::fileName));
}

bool AudiumEngine::projectChangedOnDisk() const
{
    if (currentProjectFile == File())
        return false;
    return projectFileStore->projectChangedOnDisk(currentProjectFile);
}

bool AudiumEngine::analysisChangedOnDisk() const
{
    if (currentProjectFile == File())
        return false;
    return projectFileStore->analysisChangedOnDisk(projectDirectory.getChildFile(AnalysisCache::fileName));
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
