/*
  ==============================================================================

    AudiumEngine.cpp
    Created: 29 Jan 2023 12:31:48pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudiumEngine.h"
#include "Util/Preferences.h"

const char* AudiumEngine::projectFileExtension = ".audium";

AudiumEngine::AudiumEngine(std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager,
                           std::shared_ptr<AudioResourceContainer> audioResourceContainer,
                           std::shared_ptr<AudioRegionContainer> audioRegionContainer,
                           std::shared_ptr<PlayListContainer> playListContainer,
                           std::shared_ptr<TransportSourceProvider> transportSourceProvider,
                           std::shared_ptr<PlayListScheduler> playListScheduler) :
    audioDeviceManager(audioDeviceManager),
    audioResourceContainer(audioResourceContainer),
    audioRegionContainer(audioRegionContainer),
    playListContainer(playListContainer),
    transportSourceProvider(transportSourceProvider),
    playListScheduler(playListScheduler)
{
}

AudiumEngine::~AudiumEngine()
{
}

void AudiumEngine::initialise()
{
    /** Resets everything to a default device setup, clearing any stored settings. */
    auto result = audioDeviceManager->initialiseWithDefaultDevices (0, 2);
    std::cout << result.toStdString() << std::endl;
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
            /// TODO: std::move (callback)
            readFromStream(inputStream);
        }
    }
    currentFile = file;
}

void AudiumEngine::saveFile (const juce::File& file, std::function<void (bool)> callback)
{
    if (! file.exists())
    {
        auto result = file.create();
        if (result != juce::Result::ok())
        {
            if (callback != nullptr)
                callback (false);
        
            return;
        }
    }
    
    juce::TemporaryFile temp (file);
    
    {
        juce::FileOutputStream out (temp.getFile());

        if (! (out.openedOk()))
        {
            return;
        }
        
        //callback(writeToStream(fo));
        writeToStream(out);
    }

    temp.overwriteTargetFileWithTemporary();
    currentFile = file;
}

bool AudiumEngine::writeToStream (juce::OutputStream& out)
{
    out.writeString ("AudiumEngineFormat");
    audioResourceContainer->writeToStream(out);
    audioRegionContainer->writeToStream(out);
    playListContainer->writeToStream(out);
    return true;
}

bool AudiumEngine::readFromStream (juce::InputStream& in)
{
    auto name = in.readString();
    jassert(name == "AudiumEngineFormat");

    if (audioResourceContainer->readFromStream(in))
    {
        if (audioRegionContainer->readFromStream(in))
        {
            if (playListContainer->readFromStream(in))
            {
                return true;
            }
        }
    }
    
    return false;
}

void AudiumEngine::cleanup()
{
    playListContainer->cleanup();
    audioRegionContainer->cleanup();
    audioResourceContainer->cleanup();
    
    currentFile = File();
}
