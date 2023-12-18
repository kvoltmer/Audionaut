/*
  ==============================================================================

    LinkAudioDevice.cpp
    Created: 25 Oct 2023 5:14:38pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "LinkAudioDevice.h"
#include "LinkEngine.hpp"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/AudioResourceContainer.h"
#include "Engine/Provider/TempoProvider.h"

LinkAudioDevice::LinkAudioDevice(std::shared_ptr<audium::LinkEngine> linkEngine,
                                 std::shared_ptr<PlayListScheduler> playListScheduler,
                                 std::shared_ptr<AudioResourceContainer> audioResourceContainer) :
    linkEngine(linkEngine),
    playListScheduler(playListScheduler),
    audioResourceContainer(audioResourceContainer)
{
}

LinkAudioDevice::~LinkAudioDevice()
{
}

void LinkAudioDevice::audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                                          int totalNumInputChannels,
                                                          float* const* outputChannelData,
                                                          int totalNumOutputChannels,
                                                          int numSamples,
                                                          [[maybe_unused]] const juce::AudioIODeviceCallbackContext& context)
{
    
    // clear output
    for (int i = 0; i < totalNumOutputChannels; ++i)
        if (outputChannelData[i] != nullptr)
            juce::zeromem (outputChannelData[i], (size_t) numSamples * sizeof (float));
    
    if (not byPass)
    {
        // Synchronize host time to reference the point when its output reaches the speaker.
        const auto hostTime =  host_time_filter.sampleTimeToHostTime(sample_time);
        const auto bufferBeginAtOutput = hostTime + linkEngine->mOutputLatency.load();
        
        linkEngine->audioCallback(bufferBeginAtOutput, numSamples);
        
        for (int i = 0; i < totalNumOutputChannels; ++i)
        {
            if (outputChannelData[i] != nullptr)
            {
                for (auto j = 0; j < numSamples; ++j)
                {
                    outputChannelData[i][j] += linkEngine->mBuffer[j];
                }
            }
        }
        
        juce::AudioBuffer<float> buffer (outputChannelData, totalNumOutputChannels, numSamples);
        juce::AudioSourceChannelInfo info (&buffer, 0, numSamples);
        playListScheduler->audioCallback(info);
        
        sample_time += numSamples;
    }
}

void LinkAudioDevice::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    sampleRate = device->getCurrentSampleRate();
    linkEngine->setSampleRate(sampleRate);
    bufferSize = device->getCurrentBufferSizeSamples();
    linkEngine->setBufferSize(bufferSize);
    
    if (playListScheduler != nullptr)
    {
        playListScheduler->prepareToPlay(sampleRate, bufferSize);
    }
    
    audioResourceContainer->prepareToPlay(sampleRate, bufferSize);
    
    auto deviceLatency = device->getOutputLatencyInSamples();
    
    std::cout << "OUTPUT DEVICE LATENCY: " << deviceLatency << " samples" << std::endl;
    using namespace std::chrono;
    const double latency = static_cast<double>(deviceLatency) / linkEngine->mSampleRate;
    linkEngine->mOutputLatency.store(duration_cast<microseconds>(duration<double>{latency}));
}

void LinkAudioDevice::audioDeviceStopped()
{
}

void LinkAudioDevice::startPlaying()
{
    linkEngine->startPlaying();
}

void LinkAudioDevice::stopPlaying()
{
    linkEngine->stopPlaying();
}
