//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Lola uses a GPL/commercial licence - see LICENCE.md for details.

#include "AudiumEngine.h"
#include "Util/Preferences.h"
#include "Engine/AutoEdit/AutoEdit.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/Link/LinkAudioDevice.h"
#include "Engine/Factory/AudioTrackFactory.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/AudioSources/AudiumTransportSource.h"
#include "Engine/Export/AudioExportThread.h"

#include "Interface/ColourIds.h"

namespace audium {

const char* AudiumEngine::projectFileExtension = ".audium";
const char* AudiumEngine::projectFileName = "Project.json";
juce::File AudiumEngine::projectDirectory = File();
juce::File AudiumEngine::tempDirectory = File();

AudiumEngine::~AudiumEngine()
{
    cleanup();
}

void AudiumEngine::initialise()
{
    auto numInputChannelsNeeded = 0;
    auto numOutputChannelsNeeded = 2;
    String result;
    
    if (Preferences::valueExists(PreferenceKeys::audioDeviceSettings)) {
        
        juce::XmlDocument xml (Preferences::getValue(PreferenceKeys::audioDeviceSettings));
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
    std::cout << result.toStdString() << std::endl;
    audioDeviceManager->addAudioCallback(linkAudioDevice.get());
}

void AudiumEngine::uninitialise()
{
    
    if (auto stateXml = audioDeviceManager->createStateXml()) {
        Preferences::setValue(PreferenceKeys::audioDeviceSettings, stateXml->toString());
    }
    
    undoManager->clearUndoHistory();
    audioDeviceManager->removeAudioCallback(linkAudioDevice.get());
    
}

void AudiumEngine::cleanup()
{
    audioTrackContainer->cleanup();
    audioResourceContainer->cleanup();
    undoManager->clearUndoHistory();
    
    currentProjectFile = File();
}

void AudiumEngine::createNewProject()
{
    AudioResourceContainer::createTemporaryProjectDirectory(true);
    
    audium::WaveFormColours::resetWaveFormColour();
    for (auto i = 0; i < 1; i++) {
        auto track = audioTrackContainer->createNewAudioTrack("Track " + String(i+1));
        track->setColour(audioTrackContainer->getNewAudioTrackColour());
        track->addChannel();
        track->addChannel();
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
                                                    getPlayListScheduler()->isArrangementMode(),
                                                    callback);
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
                if (readFromStream(inputStream, true)){
                    currentProjectFile = inFile;
                    undoManager->clearUndoHistory();
                    playListScheduler->commitPlayListData();
                    return true;
                }
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
            AudioResourceContainer::copyOrMoveAudioFiles(sourceDirectory, destinationDirectory);
            audioResourceContainer->changeAudioFilePaths(destinationDirectory);
        }
        // assign new project directory
        projectDirectory = file.getParentDirectory();
        
        juce::TemporaryFile temp (file);
        juce::FileOutputStream out (temp.getFile());
        if (out.failedToOpen()) {
            NullCheckedInvocation::invoke (callback, out.getStatus().getErrorMessage().toStdString());
            return false;
        }
        
        if (writeToStream(out)) {
            if (temp.overwriteTargetFileWithTemporary()) {
                currentProjectFile = file;
                undoManager->clearUndoHistory();
                return true;
            }
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
    
    return audioTrackContainer->readFromJson(jsonAudium, rebuild);
}

int AudiumEngine::getSizeInUnits()
{
    return audioTrackContainer->getSizeInUnits() + 1;
}

void AudiumEngine::createDefaultRegionAndPlayList(std::shared_ptr<AudioTrack> track)
{
    jassertfalse;
    //    if (audioRegionContainer->getNumRegions(track.get()) == 0)
    //    {
    //        auto region = audioRegionContainer->createDefaultRegion(track);
    //        track->getPlayListContainer()->createPlayListItem(region);
    //    }
}


void AudiumEngine::invokeAutoEdit(AutoEditConfig config)
{
    // first bounce the mix
    audium::ExportAudioConfig bounceConfig;
    bounceConfig.fileName = juce::File::createTempFile(".wav");
    bounceConfig.sampleRate = 48000.0;
    
    // create the thread
    auto thread = std::make_unique<AudioExportThread>(*this, bounceConfig);
    
    // start the thread
    if (thread->runThread())
    {
        // thread finished normally..
        config.bounceFileName = bounceConfig.fileName.getFullPathName().toStdString();
        
        std::unique_ptr<AutoEdit> autoEdit(new AutoEdit(audioTrackContainer,
                                                        audioResourceContainer));
        if (autoEdit->invokeAutoEdit(config))
        {
            autoEdit->applyAutoEditResult(bounceConfig.sampleRate);
        }
    }
    else
    {
        // user pressed the cancel button..
    }
    
}

std::shared_ptr<PlayListContainer> AudiumEngine::getPlayListContainer(std::shared_ptr<AudioTrack> track) const
{
    return track->getPlayListContainer();
}

} // namespace audium
