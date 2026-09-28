//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <thread>

#include "LinkAudioDevice.h"
#include "LinkEngine.hpp"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/Provider/TempoProvider.h"
#include "Engine/Playback/AudioBusInterface.h"

namespace audium {

LinkAudioDevice::LinkAudioDevice(std::shared_ptr<audium::LinkEngine> linkEngine_,
                                 std::shared_ptr<PlayListScheduler> playListScheduler_) :
    linkEngine(linkEngine_),
    playListScheduler(playListScheduler_)
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
                                                        [[maybe_unused]] const juce::AudioIODeviceCallbackContext& context_)
{
    // Fade tails, gain ramps and the stretch/resampling filters decay towards
    // zero; flush denormals for the whole block so they cannot slow the
    // callback down (one guard per callback, not per voice).
    juce::ScopedNoDenormals noDenormals;

    dspLoadMeter.begin();
    
    // clear output
    for (int i = 0; i < totalNumOutputChannels; ++i)
        if (outputChannelData[i] != nullptr)
            juce::zeromem (outputChannelData[i], (size_t) numSamples * sizeof (float));
    
    // Announce the render path before reading the bypass (both seq_cst):
    // a bypass request stores its flag and then reads this one, so either
    // this callback sees the bypass and stays out, or the request sees
    // the callback and waits for it to finish.
    inCallback.store (true);

    if (not byPass.load()) {
        // Synchronize host time to reference the point when its output reaches the speaker.
        const auto hostTime =  host_time_filter.sampleTimeToHostTime(sample_time);
        const auto bufferBeginAtOutput = hostTime + linkEngine->mOutputLatency.load();
        const auto isPlaying = linkEngine->audioCallback(bufferBeginAtOutput,
                                                         static_cast<std::size_t>(numSamples));
        const auto beats = linkEngine->beatAtTime(bufferBeginAtOutput, linkEngine->quantum());
        
        // Note: like inBuf below, setDataToReferTo stays allocation-free for up to 32
        // channels (juce::AudioBuffer's preallocated channel-pointer space).
		jassert (totalNumOutputChannels > 0);
		outBuf.setDataToReferTo(outputChannelData, totalNumOutputChannels, numSamples);
        juce::dsp::AudioBlock<float> out (outBuf);
        

		if (totalNumInputChannels > 0)
		    inBuf.setDataToReferTo(inputChannelData, totalNumInputChannels, numSamples);
        else {
           // inBuf.setSize(1, numSamples); // dummy input buffer with 1 channel and numSamples samples
        }
        juce::dsp::AudioBlock<const float> in (inBuf);
    
        juce::dsp::ProcessContextNonReplacing<float> context (in, out);
        
        playListScheduler->process(context, isPlaying, beats, numSamples);
        
        sample_time += static_cast<std::uint64_t>(numSamples);
    }

    inCallback.store (false);
    dspLoadMeter.end (numSamples, sampleRate);
}

void LinkAudioDevice::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    sampleRate = device->getCurrentSampleRate();
    linkEngine->setSampleRate(sampleRate);
    bufferSize = device->getCurrentBufferSizeSamples();
    
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

    if (! isByPass)
        return;

    // A callback that read the flag before the store may still be
    // rendering; let it drain before the caller touches the render path
    // (it is at most one block, so a short spin is enough).
    while (inCallback.load())
        std::this_thread::yield();
}

} // namespace audium
