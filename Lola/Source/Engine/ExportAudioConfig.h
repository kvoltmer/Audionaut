/*
  ==============================================================================

    ExportAudioConfig.h
    Created: 28 Oct 2024 10:59:19am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

namespace audium
{

struct ExportAudioConfig {

    int bitDepth            = 24;
    double sampleRate       = 44100;
    int blockSize           = 1024;
    int numChannels         = 2;
    double positionSeconds  = 0.0;
    juce::File fileName;
    
    double progress = 0.0;
    std::string progressMessage;
    
};

} // namespace audium
