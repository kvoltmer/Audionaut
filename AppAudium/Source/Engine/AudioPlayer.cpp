/*
  ==============================================================================

    AudioPlayer.cpp
    Created: 23 Mar 2023 11:13:52am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudioPlayer.h"
#include "AudiumTransportSource.h"

AudioPlayer::AudioPlayer(std::shared_ptr<AudiumTransportSource> audioTransportSource,
                         std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager,
                         juce::InputSource* inputSource,
                         juce::AudioFormatManager& formatManager,
                         juce::TimeSliceThread* readAheadThread) :
    audioTransportSource(audioTransportSource),
    audioDeviceManager(audioDeviceManager)
{
    audioDeviceManager->addAudioCallback(this);
    setSource (audioTransportSource.get());
    

    auto stream = rawToUniquePtr (inputSource->createInputStream());
    jassert(stream != nullptr);
    if (stream == nullptr)
        return;

    auto reader = rawToUniquePtr (formatManager.createReaderFor (std::move (stream)));
    jassert(reader);
    if (reader == nullptr)
        return;

    audioFormatReaderSource = std::make_unique<juce::AudioFormatReaderSource> (reader.release(), true);

    sampleRate = audioFormatReaderSource->getAudioFormatReader()->sampleRate;
    numChannels = audioFormatReaderSource->getAudioFormatReader()->numChannels;
    // plug it into the transport source
    audioTransportSource->setSource (audioFormatReaderSource.get(),
                                    32768,                   // tells it to buffer this many samples ahead
                                    readAheadThread,                 // this is the background thread to use for reading-ahead
                                    sampleRate);     // allows for sample rate correction

    
    
}


AudioPlayer::~AudioPlayer()
{
    audioTransportSource->setSource (nullptr);
    setSource (nullptr);
    
    audioDeviceManager->removeAudioCallback(this);
}

//void AudioPlayer::prepareToPlay (double newSampleRate, int newBufferSize)
//{
//    audioSourcePlayer.prepareToPlay(newSampleRate, newBufferSize);
//}

void AudioPlayer::audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                       int totalNumInputChannels,
                                       float* const* outputChannelData,
                                       int totalNumOutputChannels,
                                       int numSamples,
                                       const juce::AudioIODeviceCallbackContext& context)
{
    if (not byPass)
    {
        juce::AudioSourcePlayer::audioDeviceIOCallbackWithContext(inputChannelData, totalNumInputChannels, outputChannelData, totalNumOutputChannels, numSamples, context);
    }
}

void AudioPlayer::renderOffline(float* const* outputChannelData, int totalNumOutputChannels, int numSamples)
{
    
    juce::AudioIODeviceCallbackContext context;
    juce::AudioSourcePlayer::audioDeviceIOCallbackWithContext(nullptr, 0, outputChannelData, totalNumOutputChannels, numSamples, context);
    
}
