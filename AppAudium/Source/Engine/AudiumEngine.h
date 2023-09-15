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
#include "AudioResourceContainer.h"
#include "AudioRegionContainer.h"
#include "Engine/PlayList/PlayListContainer.h"

class TransportSourceProvider;
class PlayListScheduler;
struct AutoEditConfig;

/// The Audium engine
class AudiumEngine {
    
public:
    AudiumEngine(std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager,
                 std::shared_ptr<AudioResourceContainer> audioResourceContainer,
                 std::shared_ptr<AudioRegionContainer> audioRegionContainer,
                 std::shared_ptr<PlayListContainer> playListContainer,
                 std::shared_ptr<TransportSourceProvider> transportSourceProvider,
                 std::shared_ptr<PlayListScheduler> playListScheduler);
    ~AudiumEngine();
    
    void initialise();
    
    void openFile (const juce::File& file, std::function<void (bool)> callback);
    void saveFile (const juce::File& file, std::function<void (bool)> callback);
    
    bool writeToStream (juce::OutputStream& outputStream);
    bool readFromStream (juce::InputStream& inputStream);
    
    static const char* projectFileExtension;
    
    const juce::File getCurrentFile() const { return currentFile; }
    
    void cleanup();
    
    std::shared_ptr<AudioResourceContainer> getAudioResourceContainer() const { return audioResourceContainer; }
    std::shared_ptr<AudioRegionContainer> getAudioRegionContainer() const { return audioRegionContainer; }
    std::shared_ptr<PlayListContainer> getPlayListContainer() const { return playListContainer; }
    std::shared_ptr<TransportSourceProvider> getTransportSourceProvider() const { return transportSourceProvider; }
    std::shared_ptr<PlayListScheduler> getPlayListScheduler() const { return playListScheduler; }
    
    void invokeAutoEdit(const AutoEditConfig config);
    
private:
    std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager;
    std::shared_ptr<AudioResourceContainer> audioResourceContainer;
    std::shared_ptr<AudioRegionContainer> audioRegionContainer;
    std::shared_ptr<PlayListContainer> playListContainer;
    std::shared_ptr<TransportSourceProvider> transportSourceProvider;
    std::shared_ptr<PlayListScheduler> playListScheduler;
    
    juce::File currentFile;
    

    /// TODO: thread save container
    // std::is_trivially_copyable
    // std::array, has a static size set at compile time. It does not have internal pointers and can therefore be copied simply by using memcpy. It therefore is trivial to copy.
    std::atomic<std::array<int, 3>> test;
    
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudiumEngine)
};
