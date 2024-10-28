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
    int numChannels         = 2;
    juce::File fileName;
    
};

} // namespace audium
