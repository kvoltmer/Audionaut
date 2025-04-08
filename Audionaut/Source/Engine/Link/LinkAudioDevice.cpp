//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "LinkAudioDevice.h"
#include "LinkEngine.hpp"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/Provider/TempoProvider.h"

namespace audium {

LinkAudioDevice::LinkAudioDevice(std::shared_ptr<audium::LinkEngine> linkEngine,
                                 std::shared_ptr<PlayListScheduler> playListScheduler) :
linkEngine(linkEngine),
playListScheduler(playListScheduler)
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
    
    if (not byPass.load())
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
        playListScheduler->processAudio(info);
        
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
        playListScheduler->prepareToPlay(bufferSize, sampleRate);
    }
    
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

void LinkAudioDevice::setBypass(bool isByPass)
{
    byPass.store(isByPass);
}

} // namespace audium
