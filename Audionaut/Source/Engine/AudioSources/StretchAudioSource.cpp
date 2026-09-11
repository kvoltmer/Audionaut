//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "StretchAudioSource.h"
#include "Engine/PlayList/ClipSpeed.h"

namespace audium {

StretchAudioSource::StretchAudioSource (juce::AudioSource* inputSource, int numChannels_) :
    input (inputSource),
    numChannels (numChannels_)
{
    jassert (input != nullptr);
    jassert (numChannels > 0);
}

StretchAudioSource::~StretchAudioSource() = default;

void StretchAudioSource::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    input->prepareToPlay (samplesPerBlockExpected, sampleRate);

    preparedBlockSize = samplesPerBlockExpected;

    // the engine is picked here, off the audio thread; the backend allocates
    // everything it will ever need now
    preparedEngine = StretchEngines::getSelected();
    backend = StretchEngines::create (preparedEngine);
    backend->prepare (numChannels, sampleRate, samplesPerBlockExpected, ClipSpeed::maxSpeedRatio);

    // The scratch must fit the biggest single pull the backend may ask for.
    const auto maxPull = backend->maxInputLength (samplesPerBlockExpected, ClipSpeed::maxSpeedRatio);
    inputScratch.setSize (numChannels, juce::jmax (1, maxPull));
    inputScratch.clear();

    needsPriming.store (true);
}

void StretchAudioSource::releaseResources()
{
    input->releaseResources();
}

void StretchAudioSource::pullInput (int numSamples)
{
    for (int done = 0; done < numSamples;)
    {
        const auto chunk = juce::jmin (preparedBlockSize, numSamples - done);
        juce::AudioSourceChannelInfo chunkInfo (&inputScratch, done, chunk);
        input->getNextAudioBlock (chunkInfo);
        done += chunk;
    }
}

void StretchAudioSource::prime (double ratio)
{
    // look-ahead for the backend: the first output after this starts at
    // the upstream position the pull begins from
    const auto primeSamples = juce::jlimit (0, inputScratch.getNumSamples(),
                                            backend->primeInputLength (ratio));
    pullInput (primeSamples);
    backend->prime (inputScratch.getArrayOfReadPointers(), primeSamples, ratio);
}

void StretchAudioSource::getNextAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    if (! enabled.load())
    {
        input->getNextAudioBlock (info);
        return;
    }

    if (backend == nullptr)
    {
        info.clearActiveBufferRegion();
        return;
    }

    const auto ratio = speedRatio.load();

    if (needsPriming.exchange (false))
        prime (ratio);

    const auto inputSamples = juce::jlimit (0, inputScratch.getNumSamples(),
                                            backend->inputForOutput (info.numSamples, ratio));
    pullInput (inputSamples);

    const auto outputChannels = juce::jmin (numChannels, info.buffer->getNumChannels());

    // The stretcher wants exactly its configured channel count; a surplus
    // configured channel (mono buffer under a stereo chain cannot happen -
    // the transport sizes buffers by the reader) writes into the scratch.
    float* outputs[64];
    jassert (numChannels <= 64);
    for (int channel = 0; channel < numChannels; ++channel)
        outputs[channel] = channel < outputChannels
            ? info.buffer->getWritePointer (channel, info.startSample)
            : inputScratch.getWritePointer (channel);

    backend->process (inputScratch.getArrayOfReadPointers(), inputSamples,
                      outputs, info.numSamples, ratio);

    // channels beyond the chain's count carry stale data in this path
    for (int channel = numChannels; channel < info.buffer->getNumChannels(); ++channel)
        info.buffer->clear (channel, info.startSample, info.numSamples);
}

} // namespace audium
