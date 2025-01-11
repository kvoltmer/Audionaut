/*
  ==============================================================================

    AudiumEngine.cpp
    Created: 29 Jan 2023 12:31:48pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

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

const char* AudiumEngine::projectFileExtension = ".audium";
juce::File AudiumEngine::projectDirectory = File();

AudiumEngine::~AudiumEngine()
{
    cleanup();
}

void AudiumEngine::initialise()
{
    /** Resets everything to a default device setup, clearing any stored settings. */
    auto result = audioDeviceManager->initialiseWithDefaultDevices (0, 2);
    std::cout << result.toStdString() << std::endl;
    
    audioDeviceManager->addAudioCallback(linkAudioDevice.get());
}

void AudiumEngine::uninitialise()
{
    undoManager->clearUndoHistory();
    audioDeviceManager->removeAudioCallback(linkAudioDevice.get());
    
}

void AudiumEngine::cleanup()
{
    audioTrackContainer->cleanup();
    audioResourceContainer->cleanup();
    undoManager->clearUndoHistory();
    currentFile = File();
}

void AudiumEngine::createNewProject()
{
    audium::WaveFormColours::resetWaveFormColour();
    for (auto i = 0; i < 1; i++) {
        auto track = audioTrackContainer->createNewAudioTrack("Track " + String(i+1));
        track->setColour(audioTrackContainer->getNewAudioTrackColour());
        track->addChannel();
        track->addChannel();
    }
}

void AudiumEngine::openFile (const juce::File& file, std::function<void (bool,std::string)> callback)
{
    try
    {
        if (file.exists() &&
            file.hasFileExtension (projectFileExtension))
        {
            juce::FileInputStream inputStream(file);
            if (inputStream.openedOk())
            {
                projectDirectory = file.getParentDirectory();
                if (readFromStream(inputStream, true))
                {
                    currentFile = file;
                    undoManager->clearUndoHistory();
                    playListScheduler->commitPlayListData();
                    NullCheckedInvocation::invoke (callback, true, "");
                    return;
                }
            }
            
            // we failed to read :(
            cleanup();
            NullCheckedInvocation::invoke (callback, false, "unknown error");
            
        }
    }
    catch (std::exception &e)
    {
        cleanup();
        std::cout << e.what() << std::endl;
        NullCheckedInvocation::invoke (callback, false, e.what());
    }
    
}

void AudiumEngine::saveFile (const juce::File& file_, std::function<void (bool,std::string)> callback)
{
    try
    {
        auto file = file_;
        
        if (! file.hasFileExtension (projectFileExtension))
            file = juce::File(file.getFullPathName() + projectFileExtension);
        
        if (! file.exists())
            file.create();
        
        projectDirectory = file.getParentDirectory();
        
        juce::TemporaryFile temp (file);
            
        juce::FileOutputStream out (temp.getFile());

        if (out.failedToOpen()) {
            NullCheckedInvocation::invoke (callback, false, out.getStatus().getErrorMessage().toStdString());
            return;
        }

        if (writeToStream(out)) {
            if (temp.overwriteTargetFileWithTemporary()) {
                currentFile = file;
                undoManager->clearUndoHistory();
                NullCheckedInvocation::invoke (callback, true, "");
                return;
            }
        }
        
        jassertfalse;
        NullCheckedInvocation::invoke (callback, false, "unknown error");
    }
    catch (std::exception &e)
    {
        std::cout << e.what() << std::endl;
        NullCheckedInvocation::invoke (callback, false, e.what());
    }
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
    if (audium::Streamable::readFromStream(inputStream))
    {
        return true;
    }
    return false;
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

    std::cout << std::setw(2) << output << std::endl;
    return true;
}

bool AudiumEngine::readFromJson (json& input, bool rebuild)
{
    cleanup();
    
    auto jsonAudium = input["audium"];
    
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
