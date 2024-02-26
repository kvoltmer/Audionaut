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
#include "Engine/Group/AudioGroupContainer.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/Link/LinkAudioDevice.h"
#include "Engine/Factory/AudioGroupFactory.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/AudiumTransportSource.h"

const char* AudiumEngine::projectFileExtension = ".audium";

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
    audioGroupContainer->cleanup();
    audioResourceContainer->cleanup();
    audioRegionContainer->cleanup();
    undoManager->clearUndoHistory();
    currentFile = File();
}

void AudiumEngine::openFile (const juce::File& file, std::function<void (bool)> callback)
{
    if (! file.exists())
    {
        if (callback != nullptr)
            callback (false);

        return;
    }
    
    if (file.hasFileExtension (projectFileExtension))
    {
        juce::FileInputStream inputStream(file);
        if (inputStream.openedOk())
        {
            undoManager->clearUndoHistory();

            if (readFromStream(inputStream))
            {
                currentFile = file;
            }
            else
            {
                callback (false);
            }
        }
    }
    else
    {
        std::cout << "error: missing project file extension" << std::endl;
    }
    
}

bool AudiumEngine::saveFile (const juce::File& f)
{
    juce::File file = f;
    
    if (! file.hasFileExtension (projectFileExtension))
    {
        file = juce::File(file.getFullPathName() + projectFileExtension);
    }
    
    if (! file.exists())
    {
        auto result = file.create();
        if (result != juce::Result::ok())
        {
            return false;
        }
    }
    
    juce::TemporaryFile temp (file);
        
    juce::FileOutputStream out (temp.getFile());

    if (! (out.openedOk()))
    {
        std::cout << out.getStatus().getErrorMessage().toStdString() << std::endl;
        return false;
    }
    
    writeToStream(out);
    undoManager->clearUndoHistory();
    

    if (temp.overwriteTargetFileWithTemporary())
    {
        currentFile = file;
        return true;
    }
    return false;
}

void AudiumEngine::setBypass(bool bypass)
{
    linkAudioDevice->setBypass(bypass);
}

void AudiumEngine::bounceToFile(const juce::File& f, std::function<void (bool)> callback,
                                double preferedSampleRate,
                                bool defaultGroupOnly)
{
    std::cout << "bounce -> " << f.getFullPathName().toStdString() << std::endl;
    
    auto numSamples = audioDeviceManager->getCurrentAudioDevice()->getCurrentBufferSizeSamples();
    double sampleRate = preferedSampleRate;
        
    playListScheduler->prepareToPlay(sampleRate, numSamples);
    audioResourceContainer->prepareToPlay(sampleRate, numSamples);
    
    
    auto numOutputChannels = 2;
    
    setBypass(true);

    juce::TemporaryFile tempFile (f);
    std::unique_ptr<OutputStream> outStream (tempFile.getFile().createOutputStream());

    if (outStream != nullptr)
    {
        const StringPairArray metadata;
        WavAudioFormat wav;
        std::unique_ptr<AudioFormatWriter> writer (wav.createWriterFor (outStream.get(), sampleRate,
                                                                        numOutputChannels, (int) 32,
                                                                        metadata, 0));
        if (writer != nullptr)
        {
            outStream.release();
            
            playListScheduler->bounceToFile(writer.get(), sampleRate, numSamples, numOutputChannels);

            writer.reset();
            tempFile.overwriteTargetFileWithTemporary();
        }
    }
    
    // change back to device settings
    numSamples = audioDeviceManager->getCurrentAudioDevice()->getCurrentBufferSizeSamples();
    sampleRate = audioDeviceManager->getCurrentAudioDevice()->getCurrentSampleRate();
    playListScheduler->prepareToPlay(sampleRate, numSamples);
    audioResourceContainer->prepareToPlay(sampleRate, numSamples);
    
    
    setBypass(false);
    std::cout << "done" << std::endl;
}

bool AudiumEngine::writeToStream (juce::OutputStream& out)
{
    out.writeString ("AudiumEngineFormat");
    
    // Tempo
    out.writeDouble(playListScheduler->getTempoProvider()->getTempo());
    
    // Groups
    audioGroupContainer->writeToStream(out);
        
    return true;
}

bool AudiumEngine::readFromStream (juce::InputStream& in)
{
    auto name = in.readString();
    jassert(name == "AudiumEngineFormat");
    
    cleanup();
    
    while (! in.isExhausted())
    {
        // Tempo
        auto tempo = in.readDouble();
        
        if (!linkAudioDevice->getLinkEngine()->isEnabled()) // don't interfere with running sessions
            playListScheduler->getTempoProvider()->setTempo(tempo);
        
        // Groups
        if (audioGroupContainer->readFromStream(in))
        {
            return true;
        }
        return true;
    }
    return false;
}

void AudiumEngine::createDefaultRegionAndPlayList(std::shared_ptr<AudioGroup> group)
{
    if (audioRegionContainer->getNumRegions(group.get()) == 0)
    {
        auto region = audioRegionContainer->createDefaultRegion(group);
        group->getPlayListContainer()->createPlayListItem(region);
    }
}


void AudiumEngine::invokeAutoEdit(AutoEditConfig config)
{
    double sampleRate = 48000.0;
    
    // first bounce the mix
    const auto bounceFile = juce::File::createTempFile(".wav");
     
    bounceToFile(bounceFile, nullptr, sampleRate);
    config.bounceFileName = bounceFile.getFullPathName().toStdString();
    
#if 0
    // erase everything
    cleanup();
    
    // create a fresh resource
    const auto bounceUrl = juce::URL(bounceFile);
    auto audioGroup = audioGroupContainer->createNewAudioGroup(*getAudioResourceContainer(), *getAudioRegionContainer(), bounceUrl.getFileName().toStdString());
    auto subGroup = audioGroup->createNewAudioSubGroup();
    audioResourceContainer->addAudioResource(bounceUrl, audioGroup, subGroup);
#endif
    
    std::unique_ptr<AutoEdit> autoEdit(new AutoEdit(audioGroupContainer,
                                                    audioRegionContainer,
                                                    audioResourceContainer));
    if (autoEdit->invokeAutoEdit(config))
    {
        autoEdit->applyAutoEditResult(sampleRate);
    }
}

std::shared_ptr<PlayListContainer> AudiumEngine::getPlayListContainer(std::shared_ptr<AudioGroup> group) const
{
    return group->getPlayListContainer();
}
