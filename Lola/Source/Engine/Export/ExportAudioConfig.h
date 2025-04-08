//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

namespace audium {

struct ExportAudioConfig {

    int bitDepth            = 24;
    double sampleRate       = 44100;
    int blockSize           = 1024;
    int numChannels         = 2;
    bool multiMono          = false;
    double positionSeconds  = 0.0;
    juce::File fileName;
    
    double progress = 0.0;
    std::string progressMessage;
    
};

} // namespace audium
