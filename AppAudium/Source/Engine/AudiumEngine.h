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

/// The Audium engine
class AudiumEngine {
    
public:
    AudiumEngine(std::shared_ptr<AudioResourceContainer> container);
    ~AudiumEngine();
    
    std::shared_ptr<AudioResourceContainer> getAudioResourceContainer() { return audioResourceContainer; }
    
    void openFile (const juce::File& file, std::function<void (bool)> callback);
    void saveFile (const juce::File& file, std::function<void (bool)> callback);
    
    bool writeToStream (juce::OutputStream& outputStream);
    bool readFromStream (juce::InputStream& inputStream);
    
    static const char* projectFileExtension;
    
    const juce::File getCurrentFile() const { return currentFile; }
    
private:
    std::shared_ptr<AudioResourceContainer> audioResourceContainer;
    
    juce::File currentFile;
    
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudiumEngine)
};
