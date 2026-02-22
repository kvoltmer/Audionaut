//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

// Make sure to define this before <cmath> is included for Windows
#define _USE_MATH_DEFINES
#include <ableton/Link.hpp>
#include <ableton/link/HostTimeFilter.hpp>
#include "LinkEngine.hpp"

namespace audium {

class PlayListScheduler;
class AudioResourceContainer;
class TransportSourceContainer;

class LinkAudioDevice : public juce::AudioIODeviceCallback {
    
public:
    LinkAudioDevice(std::shared_ptr<audium::LinkEngine> linkEngine,
                    std::shared_ptr<PlayListScheduler> playListScheduler);
    ~LinkAudioDevice() override;
    
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                           int totalNumInputChannels,
                                           float* const* outputChannelData,
                                           int totalNumOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override;
    
    
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    
    void startPlaying();
    void stopPlaying();
    
    void setBypass(bool isByPass);
    
    audium::LinkEngine* getLinkEngine() const { return linkEngine.get(); }
    
private:
    std::shared_ptr<audium::LinkEngine> linkEngine;
    std::shared_ptr<PlayListScheduler> playListScheduler;
    
    ableton::link::HostTimeFilter<ableton::link::platform::Clock> host_time_filter;
    std::uint64_t sample_time = 0;
    double sampleRate = 0.0;
    int bufferSize = 0;
    std::atomic<bool> byPass;
    
};

} // namespace audium
