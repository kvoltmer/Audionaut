/*
  ==============================================================================

    AudiumEngine.h
    Created: 29 Jan 2023 12:32:40pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <memory>
#include <JuceHeader.h>

#include "Engine/Streamable.h"
#include "Engine/TimeContext.h"

class AudioGroupContainer;
class AudioGroup;
class PlayListContainer;
class AudioRegionContainer;
class AudioResourceContainer;
class TransportSourceContainer;
class PlayListScheduler;
struct AutoEditConfig;
class LinkAudioDevice;

/// The Audium engine
class AudiumEngine : public audium::Streamable
{
    
public:
    AudiumEngine(std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager,
                 std::shared_ptr<AudioGroupContainer> audioGroupContainer,
                 std::shared_ptr<AudioResourceContainer> audioResourceContainer,
                 std::shared_ptr<AudioRegionContainer> audioRegionContainer,
                 std::shared_ptr<PlayListScheduler> playListScheduler,
                 std::shared_ptr<LinkAudioDevice> linkAudioDevice,
                 std::shared_ptr<juce::UndoManager> undoManager) :
        audioDeviceManager(audioDeviceManager),
        audioGroupContainer(audioGroupContainer),
        audioResourceContainer(audioResourceContainer),
        audioRegionContainer(audioRegionContainer),
        playListScheduler(playListScheduler),
        linkAudioDevice(linkAudioDevice),
        undoManager(undoManager)
    {
    }
    
    ~AudiumEngine();
    
    void initialise();
    void uninitialise();
    void cleanup();
    
    void openFile (const juce::File& file, std::function<void (bool,std::string)> callback);
    bool saveFile (const juce::File& file);
    void bounceToFile(const juce::File& f, std::function<void (bool)> callback,
                      double preferedSampleRate,
                      bool defaultGroupOnly = false);
    
    bool writeToStream (juce::OutputStream& outputStream) override;
    bool readFromStream (juce::InputStream& inputStream) override;
    bool writeToJson (json& output) override;
    bool readFromJson (json& input) override;
    int getSizeInUnits() override;
    
    void createDefaultRegionAndPlayList(std::shared_ptr<AudioGroup> group);
    
    static const char* projectFileExtension;
    
    const juce::File getCurrentFile() const { return currentFile; }
    
    std::shared_ptr<AudioGroupContainer> getAudioGroupContainer() const { return audioGroupContainer; }
    std::shared_ptr<AudioResourceContainer> getAudioResourceContainer() const { return audioResourceContainer; }
    std::shared_ptr<AudioRegionContainer> getAudioRegionContainer() const { return audioRegionContainer; }
    std::shared_ptr<PlayListContainer> getPlayListContainer(std::shared_ptr<AudioGroup> group) const;
    std::shared_ptr<PlayListScheduler> getPlayListScheduler() const { return playListScheduler; }
    std::shared_ptr<juce::UndoManager> getUndoManager() const { return undoManager; }

    void invokeAutoEdit(const AutoEditConfig config);
    
    
private:
    std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager;
    std::shared_ptr<AudioGroupContainer> audioGroupContainer;
    std::shared_ptr<AudioResourceContainer> audioResourceContainer;
    std::shared_ptr<AudioRegionContainer> audioRegionContainer;
    std::shared_ptr<PlayListScheduler> playListScheduler;
    std::shared_ptr<LinkAudioDevice> linkAudioDevice;
    std::shared_ptr<juce::UndoManager> undoManager;
    
    juce::File currentFile;

    
    /// TODO: thread save container
    // std::is_trivially_copyable
    // std::array, has a static size set at compile time. It does not have internal pointers and can therefore be copied simply by using memcpy. It therefore is trivial to copy.
    std::atomic<std::array<int, 3>> test;
    
    void setBypass(bool bypass);

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudiumEngine)
};
