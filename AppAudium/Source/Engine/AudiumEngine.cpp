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
#include "Engine/AudioGroupContainer.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/Link/LinkAudioDevice.h"

const char* AudiumEngine::projectFileExtension = ".audium";

AudiumEngine::AudiumEngine(std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager,
                           std::shared_ptr<AudioGroupContainer> audioGroupContainer,
                           std::shared_ptr<AudioResourceContainer> audioResourceContainer,
                           std::shared_ptr<AudioRegionContainer> audioRegionContainer,
                           std::shared_ptr<PlayListScheduler> playListScheduler,
                           std::shared_ptr<LinkAudioDevice> linkAudioDevice) :
    audioDeviceManager(audioDeviceManager),
    audioGroupContainer(audioGroupContainer),
    audioResourceContainer(audioResourceContainer),
    audioRegionContainer(audioRegionContainer),
    playListScheduler(playListScheduler),
    linkAudioDevice(linkAudioDevice)
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
    
    audioDeviceManager->addAudioCallback(linkAudioDevice.get());
}

void AudiumEngine::uninitialise()
{
    audioDeviceManager->removeAudioCallback(linkAudioDevice.get());
    
}

void AudiumEngine::cleanup()
{
    audioGroupContainer->cleanup();
    audioResourceContainer->cleanup();
    audioRegionContainer->cleanup();
    
    currentFile = File();
}

void AudiumEngine::startPlaying()
{
    playListScheduler->startPlaying();
}

void AudiumEngine::stopPlaying()
{
    playListScheduler->stopPlaying();
}

bool AudiumEngine::isPlaying() const
{
    return playListScheduler->isPlaying();
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

void AudiumEngine::setBypass(bool bypass)
{
    linkAudioDevice->setBypass(bypass);
    for (auto r = 0; r < audioResourceContainer->getNumAudioResources(); ++r)
    {
        jassertfalse;
        //audioResourceContainer->getAudioResource(r)->getAudioPlayer()->setBypass(bypass);
    }
}
#include "Engine/AudiumTransportSource.h"

void AudiumEngine::bounceToFile(const juce::File& f, std::function<void (bool)> callback)
{
    std::cout << "bounce -> " << f.getFullPathName().toStdString() << std::endl;
    
    double sampleRate = audioDeviceManager->getCurrentAudioDevice()->getCurrentSampleRate();
    auto numSamples = audioDeviceManager->getCurrentAudioDevice()->getCurrentBufferSizeSamples();
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

            auto lastPosition = playListScheduler->getAbsolutePositionSeconds();
            playListScheduler->setAbsolutePositionSeconds(0.0);
            playListScheduler->startPlaying();
            
            auto seconds = playListScheduler->getTotalLengthSeconds();
            auto iterations = static_cast<int>(seconds * sampleRate) / numSamples;
            for (auto i = 0; i < iterations; ++i)
            {
                juce::AudioBuffer<float> buffer(numOutputChannels, numSamples);
                juce::AudioSourceChannelInfo info (&buffer, 0, numSamples);
                jassertfalse;
                /// TODO: implement
                //playListScheduler->tick(numSamples);
                
                for (auto r = 0; r < audioResourceContainer->getNumAudioResources(); ++r)
                {
                    audioResourceContainer->getAudioResource(r)->getAudioTransportSource()->getNextAudioBlock(info);
                    //audioResourceContainer->getAudioResource(r)->getAudioPlayer()->renderOffline(buffer.getArrayOfWritePointers(),
                     //                                                                            numOutputChannels, numSamples);
                }
                
                writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
                /// TODO: without waiting the output is fucked
                Time::waitForMillisecondCounter(Time::getMillisecondCounter() + 2);
                
            }
            
            playListScheduler->setAbsolutePositionSeconds(lastPosition);
            playListScheduler->stopPlaying();
            
            
            writer.reset();
            tempFile.overwriteTargetFileWithTemporary();
        }
    }
    
    setBypass(false);
    std::cout << "done" << std::endl;
}

bool AudiumEngine::writeToStream (juce::OutputStream& out)
{
    out.writeString ("AudiumEngineFormat");
    
    // 1. Groups
    audioGroupContainer->writeToStream(out);
    
    // 2. Resources
    audioResourceContainer->writeToStream(out);
    
    // 3. Regions
    audioRegionContainer->writeToStream(out);
    
    // 4. Playlists
    for (auto g = 0; g < audioGroupContainer->getNumItems(); g++)
    {
        audioGroupContainer->getAudioGroup(g)->getPlayListContainer()->writeToStream(out);
    }
    
    // 5. Other engine values
    out.writeDouble(playListScheduler->getTempo());
    
    return true;
}

bool AudiumEngine::readFromStream (juce::InputStream& in)
{
    auto name = in.readString();
    jassert(name == "AudiumEngineFormat");
    
    cleanup();
    
    while (! in.isExhausted())
    {
        // 1. Groups
        if (audioGroupContainer->readFromStream(in, *audioResourceContainer.get(), *audioRegionContainer.get()))
        {
            // 2. Resources
            if (audioResourceContainer->readFromStream(in, *this))
            {
                // 3. Regions
                if (audioRegionContainer->readFromStream(in))
                {
                    // 4. Playlists
                    for (auto g = 0; g < audioGroupContainer->getNumItems(); g++)
                    {
                        audioGroupContainer->getAudioGroup(g)->getPlayListContainer()->readFromStream(in);
                    }
                }
            }
        }
        
        // 5. Other engine values
        playListScheduler->setTempo(in.readDouble());
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
