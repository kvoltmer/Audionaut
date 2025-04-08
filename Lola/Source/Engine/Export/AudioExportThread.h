//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Engine/AudiumEngine.h"

namespace audium {

class AudioExportThread  : public juce::ThreadWithProgressWindow
{
public:
    AudioExportThread(AudiumEngine &audiumEngine_,
                      ExportAudioConfig &config_) :
    juce::ThreadWithProgressWindow ("exporting...", true, true),
    audiumEngine(audiumEngine_),
    config(config_)
    {
    }
    
    void run()
    {
        bounceToFile(config);
    }
    
    
    void bounceToFile(ExportAudioConfig &config);
    
private:
    
    AudiumEngine &audiumEngine;
    ExportAudioConfig &config;
};

} // namespace audium

