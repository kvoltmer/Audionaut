/*
  ==============================================================================

    AudioExportThread.h
    Created: 31 Oct 2024 9:40:47am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "Engine/AudiumEngine.h"

class AudioExportThread  : public juce::ThreadWithProgressWindow
{
public:
    AudioExportThread(AudiumEngine &audiumEngine_,
                      audium::ExportAudioConfig &config_) :
        juce::ThreadWithProgressWindow ("exporting...", true, true),
        audiumEngine(audiumEngine_),
        config(config_)
    {
    }

    void run()
    {
        bounceToFile(config);
    }
    
    
    void bounceToFile(audium::ExportAudioConfig &config);

private:
    
    AudiumEngine &audiumEngine;
    audium::ExportAudioConfig &config;
};

