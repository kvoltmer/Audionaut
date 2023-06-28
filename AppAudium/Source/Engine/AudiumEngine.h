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

class AudiumTransportSource;

/// The Audium engine
class AudiumEngine {
    
public:
    AudiumEngine(std::shared_ptr<AudioResourceContainer> audioResourceContainer,
                 std::shared_ptr<AudioRegionContainer> audioRegionContainer,
                 std::shared_ptr<PlayListContainer> playListContainer,
                 std::shared_ptr<AudiumTransportSource> audiumTransportSource);
    ~AudiumEngine();
    
    void openFile (const juce::File& file, std::function<void (bool)> callback);
    void saveFile (const juce::File& file, std::function<void (bool)> callback);
    
    bool writeToStream (juce::OutputStream& outputStream);
    bool readFromStream (juce::InputStream& inputStream);
    
    static const char* projectFileExtension;
    
    const juce::File getCurrentFile() const { return currentFile; }
    
    std::shared_ptr<AudioResourceContainer> getAudioResourceContainer() const { return audioResourceContainer; }
    std::shared_ptr<AudioRegionContainer> getAudioRegionContainer() const { return audioRegionContainer; }
    std::shared_ptr<PlayListContainer> getPlayListContainer() const { return playListContainer; }
    
    AudiumTransportSource* getAudiumTransportSource() const;
    
private:
    std::shared_ptr<AudioResourceContainer> audioResourceContainer;
    std::shared_ptr<AudioRegionContainer> audioRegionContainer;
    std::shared_ptr<PlayListContainer> playListContainer;
    
    juce::File currentFile;
    
    std::shared_ptr<AudiumTransportSource> audiumTransportSource;
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudiumEngine)
};
