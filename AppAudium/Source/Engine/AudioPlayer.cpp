/*
  ==============================================================================

    AudioPlayer.cpp
    Created: 23 Mar 2023 11:13:52am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudioPlayer.h"

AudioPlayer::AudioPlayer(std::shared_ptr<juce::AudioTransportSource> audioTransportSource,
                         std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager,
                         juce::InputSource* inputSource,
                         juce::AudioFormatManager& formatManager,
                         juce::TimeSliceThread* readAheadThread) :
    audioTransportSource(audioTransportSource),
    audioDeviceManager(audioDeviceManager)
{
    audioDeviceManager->addAudioCallback(&audioSourcePlayer);
    audioSourcePlayer.setSource (audioTransportSource.get());
    

    auto stream = rawToUniquePtr (inputSource->createInputStream());
    jassert(stream);
    if (stream == nullptr)
        return;

    auto reader = rawToUniquePtr (formatManager.createReaderFor (std::move (stream)));
    jassert(reader);
    if (reader == nullptr)
        return;

    audioFormatReaderSource = std::make_unique<juce::AudioFormatReaderSource> (reader.release(), true);

    sampleRate = audioFormatReaderSource->getAudioFormatReader()->sampleRate;
    // plug it into the transport source
    audioTransportSource->setSource (audioFormatReaderSource.get(),
                                    32768,                   // tells it to buffer this many samples ahead
                                    readAheadThread,                 // this is the background thread to use for reading-ahead
                                    sampleRate);     // allows for sample rate correction

    
    
}


AudioPlayer::~AudioPlayer()
{
    audioTransportSource->setSource (nullptr);
    audioSourcePlayer.setSource (nullptr);
    
    audioDeviceManager->removeAudioCallback(&audioSourcePlayer);
}

void AudioPlayer::start()
{
    audioTransportSource->start();
}

void AudioPlayer::stop()
{
    audioTransportSource->stop();
}
