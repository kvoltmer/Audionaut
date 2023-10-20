/*
  ==============================================================================

    AudioPlayer.h
    Created: 23 Mar 2023 11:13:52am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>

class AudiumTransportSource;

class AudioPlayer : public juce::AudioSourcePlayer
{

public:
    AudioPlayer(std::shared_ptr<AudiumTransportSource> audioTransportSource,
                std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager,
                juce::InputSource* inputSource,
                juce::AudioFormatManager& formatManager,
                juce::TimeSliceThread* readAheadThread);
    ~AudioPlayer();
    
    std::shared_ptr<AudiumTransportSource> getAudioTransportSource() { return audioTransportSource; }
    
    // danger!
    double sampleRate = 0.0;
    unsigned int numChannels = 0;
    
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                           int totalNumInputChannels,
                                           float* const* outputChannelData,
                                           int totalNumOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override;
    
//    void prepareToPlay (double newSampleRate, int newBufferSize);
    void renderOffline(float* const* outputChannelData, int totalNumOutputChannels, int numSamples);
    
    void setBypass(bool isByPass) { byPass = isByPass; }
    
private:
    
    std::shared_ptr<AudiumTransportSource> audioTransportSource;
    std::unique_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource;
    std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager;
    
    std::atomic<bool> byPass;

    
};
