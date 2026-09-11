//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "StretchAudioSource.h"
#include "Engine/PlayList/PlayListItem.h"

namespace audium {

StretchAudioSource::StretchAudioSource (juce::AudioSource* inputSource, int numChannels_) :
    input (inputSource),
    numChannels (numChannels_)
{
    jassert (input != nullptr);
    jassert (numChannels > 0);
}

void StretchAudioSource::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    input->prepareToPlay (samplesPerBlockExpected, sampleRate);

    preparedBlockSize = samplesPerBlockExpected;

    stretch.presetDefault (numChannels, static_cast<float> (sampleRate));

    // The scratch must fit the biggest single pull: a priming read at the
    // maximum speed, or one block's worth of input at the maximum speed.
    const auto maxPrime = stretch.outputSeekLength (static_cast<float> (PlayListItem::maxSpeedRatio));
    const auto maxBlockPull = static_cast<int> (std::ceil (samplesPerBlockExpected
                                                           * PlayListItem::maxSpeedRatio)) + 2;
    inputScratch.setSize (numChannels, juce::jmax (maxPrime, maxBlockPull));

    // Pre-warm the library's internal temp buffers off the audio thread, so
    // the first real prime and process allocate nothing.
    inputScratch.clear();
    stretch.outputSeek (inputScratch.getArrayOfWritePointers(), maxPrime);
    stretch.reset();

    inputRemainder = 0.0;
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
    // outputSeek resets, seeks past the analysis latency and discards the
    // pre-roll internally: the next process() output starts exactly at the
    // upstream position the pull below began from.
    auto primeSamples = juce::jmin (stretch.outputSeekLength (static_cast<float> (ratio)),
                                    inputScratch.getNumSamples());
    pullInput (primeSamples);
    stretch.outputSeek (inputScratch.getArrayOfWritePointers(), primeSamples);

    inputRemainder = 0.0;
}

void StretchAudioSource::getNextAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    if (! enabled.load())
    {
        input->getNextAudioBlock (info);
        return;
    }

    const auto ratio = speedRatio.load();

    if (needsPriming.exchange (false))
        prime (ratio);

    // consume ratio input samples per output sample, carrying the fraction
    const auto wanted = static_cast<double> (info.numSamples) * ratio + inputRemainder;
    auto inputSamples = juce::jmin (static_cast<int> (wanted), inputScratch.getNumSamples());
    inputRemainder = wanted - static_cast<double> (inputSamples);

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

    stretch.process (inputScratch.getArrayOfReadPointers(), inputSamples,
                     outputs, info.numSamples);

    // channels beyond the chain's count carry stale data in this path
    for (int channel = numChannels; channel < info.buffer->getNumChannels(); ++channel)
        info.buffer->clear (channel, info.startSample, info.numSamples);
}

} // namespace audium
