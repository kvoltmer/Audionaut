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

class AudioPlayer
{

public:
    AudioPlayer(std::shared_ptr<juce::AudioTransportSource> audioTransportSource,
                juce::AudioDeviceManager& audioDeviceManager,
                juce::InputSource* inputSource,
                juce::AudioFormatManager& formatManager,
                juce::TimeSliceThread* readAheadThread);
    ~AudioPlayer();
    
    void start();
    
    void stop();
    
    std::shared_ptr<juce::AudioTransportSource> getAudioTransportSource() { return audioTransportSource; }
    
private:
    juce::AudioSourcePlayer audioSourcePlayer;
    
    std::shared_ptr<juce::AudioTransportSource> audioTransportSource;
    std::unique_ptr<juce::AudioFormatReaderSource> audioFormatReaderSource;
    
    juce::AudioDeviceManager& audioDeviceManager;

};
