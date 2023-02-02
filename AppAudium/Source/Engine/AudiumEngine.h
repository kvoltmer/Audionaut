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
    
    AudioResourceContainer* getAudioResourceContainer() { return audioResourceContainer.get();}
    
    //juce::AudioFormatManager& getAudioFormatManager() { return formatManager; }
    
private:
    std::shared_ptr<AudioResourceContainer> audioResourceContainer;
    


};
