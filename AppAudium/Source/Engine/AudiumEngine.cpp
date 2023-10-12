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
#include "AudioGroupContainer.h"

const char* AudiumEngine::projectFileExtension = ".audium";

AudiumEngine::AudiumEngine(std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager,
                           std::shared_ptr<AudioGroupContainer> audioGroupContainer,
                           std::shared_ptr<AudioResourceContainer> audioResourceContainer,
                           std::shared_ptr<AudioRegionContainer> audioRegionContainer,
                           std::shared_ptr<TransportSourceContainer> transportSourceContainer,
                           std::shared_ptr<PlayListScheduler> playListScheduler) :
    audioDeviceManager(audioDeviceManager),
    audioGroupContainer(audioGroupContainer),
    audioResourceContainer(audioResourceContainer),
    audioRegionContainer(audioRegionContainer),
    transportSourceContainer(transportSourceContainer),
    playListScheduler(playListScheduler)
{
}

AudiumEngine::~AudiumEngine()
{
    cleanup();
}

void AudiumEngine::initialise()
{
    /** Resets everything to a default device setup, clearing any stored settings. */
    auto result = audioDeviceManager->initialiseWithDefaultDevices (0, 2);
    std::cout << result.toStdString() << std::endl;
}

void AudiumEngine::cleanup()
{
    audioResourceContainer->cleanup();
    audioRegionContainer->cleanup();
    audioGroupContainer->cleanup();
    
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
            /// TODO: std::move (callback)
            readFromStream(inputStream);
            currentFile = file;
        }
    }
    else
    {
        std::cout << "error: missing project file extension" << std::endl;
    }
    
}

void AudiumEngine::saveFile (const juce::File& f, std::function<void (bool)> callback)
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
    audioGroupContainer->writeToStream(out);
    
    return true;
}

bool AudiumEngine::readFromStream (juce::InputStream& in)
{
    auto name = in.readString();
    jassert(name == "AudiumEngineFormat");
    
    cleanup();

    if (audioResourceContainer->readFromStream(in, *this))
    {
        if (audioRegionContainer->readFromStream(in))
        {
            if (audioGroupContainer->readFromStream(in))
            {
                // createDefaultRegionAndPlayList();
                return true;
            }
        }
    }
    return false;
}

void AudiumEngine::createDefaultRegionAndPlayList(std::shared_ptr<AudioGroup> group)
{
    if (audioRegionContainer->getNumRegions(group.get()) == 0)
    {
        auto region = audioRegionContainer->createDefaultRegion(audioResourceContainer, group);
        group->getPlayListContainer()->createPlayListItem(region);
    }
}

void AudiumEngine::invokeAutoEdit(const AutoEditConfig config)
{
    std::unique_ptr<AutoEdit> autoEdit(new AutoEdit(audioResourceContainer,
                                                    audioRegionContainer,
                                                    audioGroupContainer->getSelectedGroup()->getPlayListContainer()));
    if (autoEdit->invokeAutoEdit(config))
    {
        autoEdit->applyAutoEditResult();
    }
}

std::shared_ptr<PlayListContainer> AudiumEngine::getPlayListContainer(std::shared_ptr<AudioGroup> group) const
{
    return group->getPlayListContainer();
}
