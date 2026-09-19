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
#include "Engine/Core/DspLoadMeter.h"

namespace audium {

class PlayListScheduler;
class AudioResourceContainer;
class VoiceSourceContainer;

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
    
    /**
        Bypassed, the callback only clears its output. Setting the bypass
        returns only once no callback is still inside the render path, so
        the caller may then touch what the audio thread uses (re-prepare,
        rebuild the project). Call from any thread but the audio thread.
    */
    void setBypass(bool isByPass);
    
    audium::LinkEngine* getLinkEngine() const { return linkEngine.get(); }

    /// Load measured across the whole audio callback; polled by the UI.
    DspLoadMeter& getDspLoadMeter() noexcept { return dspLoadMeter; }
    
private:
    std::shared_ptr<audium::LinkEngine> linkEngine;
    std::shared_ptr<PlayListScheduler> playListScheduler;
    
    ableton::link::HostTimeFilter<ableton::link::platform::Clock> host_time_filter;
    std::uint64_t sample_time = 0;
    double sampleRate = 0.0;
    int bufferSize = 0;
    std::atomic<bool> byPass { false };
    std::atomic<bool> inCallback { false };   // the render path is running
    DspLoadMeter dspLoadMeter;

    juce::AudioBuffer<const float> inBuf;
	juce::AudioBuffer<float> outBuf;
    
};

} // namespace audium
