/*
  ==============================================================================

    PlayListSchedulder.cpp
    Created: 5 Jul 2023 3:22:44pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "PlayListSchedulder.h"


PlayListScheduler::PlayListScheduler(std::shared_ptr<juce::AudioDeviceManager> audioDeviceManager) :
    audioDeviceManager(audioDeviceManager)
{
    audioDeviceManager->addAudioCallback(this);
}

PlayListScheduler::~PlayListScheduler()
{
    audioDeviceManager->removeAudioCallback(this);
}

void PlayListScheduler::audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                                          int totalNumInputChannels,
                                                          float* const* outputChannelData,
                                                          int totalNumOutputChannels,
                                                          int numSamples,
                                                          [[maybe_unused]] const juce::AudioIODeviceCallbackContext& context)
{
    // these should have been prepared by audioDeviceAboutToStart()...
    jassert (sampleRate > 0 && bufferSize > 0);

    // const juce::ScopedLock sl (readLock);

    
    // clear output
    for (int i = 0; i < totalNumOutputChannels; ++i)
        if (outputChannelData[i] != nullptr)
            juce::zeromem (outputChannelData[i], (size_t) numSamples * sizeof (float));
    
}

void PlayListScheduler::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    prepareToPlay (device->getCurrentSampleRate(),
                   device->getCurrentBufferSizeSamples());
}

void PlayListScheduler::prepareToPlay (double newSampleRate, int newBufferSize)
{
    sampleRate = newSampleRate;
    bufferSize = newBufferSize;
//    zeromem (channels, sizeof (channels));
//
//    if (source != nullptr)
//        source->prepareToPlay (bufferSize, sampleRate);
}

void PlayListScheduler::audioDeviceStopped()
{
//    if (source != nullptr)
//        source->releaseResources();

    sampleRate = 0.0;
    bufferSize = 0;

//    tempBuffer.setSize (2, 8);
}
