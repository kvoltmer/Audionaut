//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "StretchAudioSource.h"
#include "Engine/PlayList/ClipSpeed.h"

namespace audium {

StretchAudioSource::StretchAudioSource (juce::AudioSource* inputSource, int numChannels_) :
    input (inputSource),
    numChannels (numChannels_),
    backend (std::make_unique<RubberBandStretchBackend>())
{
    jassert (input != nullptr);
    jassert (numChannels > 0);
}

StretchAudioSource::~StretchAudioSource() = default;

void StretchAudioSource::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    input->prepareToPlay (samplesPerBlockExpected, sampleRate);

    preparedBlockSize = samplesPerBlockExpected;
    preparedSampleRate = sampleRate;

    // off the audio thread: the stretcher allocates everything it will
    // ever need now
    backend->prepare (numChannels, sampleRate, samplesPerBlockExpected,
                      ClipSpeed::minSpeedRatio, ClipSpeed::maxSpeedRatio);

    if (standbyInput != nullptr)
    {
        standbyInput->prepareToPlay (samplesPerBlockExpected, sampleRate);
        standbyBackend->prepare (numChannels, sampleRate, samplesPerBlockExpected,
                                 ClipSpeed::minSpeedRatio, ClipSpeed::maxSpeedRatio);
    }
    standby = {};
    prepared = true;

    // The scratch must fit the biggest single pull the stretcher may ask for.
    inputScratch.setSize (numChannels, juce::jmax (1, backend->maxInputLength()));
    inputScratch.clear();

    deferredOutputSamples = 0;
    needsPriming.store (true);
}

void StretchAudioSource::releaseResources()
{
    input->releaseResources();

    if (standbyInput != nullptr)
        standbyInput->releaseResources();
}

void StretchAudioSource::setStandbyInput (juce::AudioSource* standbyInputSource)
{
    standbyInput = standbyInputSource;
    standby = {};

    if (standbyInput == nullptr)
    {
        standbyBackend.reset();
        return;
    }

    standbyBackend = std::make_unique<RubberBandStretchBackend>();

    if (prepared)
    {
        standbyInput->prepareToPlay (preparedBlockSize, preparedSampleRate);
        standbyBackend->prepare (numChannels, preparedSampleRate, preparedBlockSize,
                                 ClipSpeed::minSpeedRatio, ClipSpeed::maxSpeedRatio);
    }
}

void StretchAudioSource::primeStandby (juce::int64 positionKey, double ratio, int blocksLeft)
{
    if (! prepared || standbyInput == nullptr)
        return;

    StandbyClaim claim (standbyBusy);
    if (! claim.held)
        return;

    if (! standby.active || standby.key != positionKey)
    {
        standby = {};
        standby.active = true;
        standby.key = positionKey;
        standby.padLeft = standbyBackend->getStartPad();
        standby.inputLeft = juce::jlimit (0, inputScratch.getNumSamples(),
                                          standbyBackend->primeInputLength (ratio));

        // spread over the calls before the jump, done one call early so a
        // little timing jitter cannot leave the prime short at the wrap.
        // Never thinner than standbyMinSlice: Rubber Band's real-time
        // engine renders a stream shifted by a sample or two when it is
        // fed in small pieces, and from about a kilosample up its output
        // matches the one-shot prime (see LoopStandbyTests).
        const auto calls = juce::jmax (1, blocksLeft - 1);
        standby.slice = juce::jmax (standbyMinSlice,
                                    (standby.padLeft + standby.inputLeft + calls - 1) / calls);

        standbyBackend->beginPrime (ratio);
    }

    if (standby.ready)
        return;

    auto budget = standby.slice;

    const auto pad = juce::jmin (standby.padLeft, budget);
    if (pad > 0)
    {
        standbyBackend->feedPadding (pad);
        standby.padLeft -= pad;
        budget -= pad;
    }

    const auto samples = juce::jmin (standby.inputLeft, budget);
    if (samples > 0)
    {
        pullInput (*standbyInput, samples);
        standbyBackend->feedInput (inputScratch.getArrayOfReadPointers(), samples);
        standby.inputLeft -= samples;
    }

    if (standby.padLeft == 0 && standby.inputLeft == 0)
        standby.ready = true;
}

bool StretchAudioSource::adoptStandby (juce::int64 positionKey) noexcept
{
    StandbyClaim claim (standbyBusy);
    if (! claim.held)
        return false;

    if (! (standby.active && standby.ready && standby.key == positionKey))
        return false;

    std::swap (input, standbyInput);
    std::swap (backend, standbyBackend);
    standby = {};
    deferredOutputSamples = 0;
    needsPriming.store (false);
    ++standbyAdoptions;
    return true;
}

void StretchAudioSource::pullInput (juce::AudioSource& from, int numSamples)
{
    for (int done = 0; done < numSamples;)
    {
        const auto chunk = juce::jmin (preparedBlockSize, numSamples - done);
        juce::AudioSourceChannelInfo chunkInfo (&inputScratch, done, chunk);
        from.getNextAudioBlock (chunkInfo);
        done += chunk;
    }
}

void StretchAudioSource::setInputReadiness (std::function<bool (int)> isReady,
                                            std::function<int()> lookAhead)
{
    inputReady = std::move (isReady);
    maxLookAhead = std::move (lookAhead);
}

void StretchAudioSource::prime (double ratio)
{
    // look-ahead for the backend: the first output after this starts at
    // the upstream position the pull begins from
    auto primeSamples = juce::jlimit (0, inputScratch.getNumSamples(),
                                      backend->primeInputLength (ratio));
    if (maxLookAhead != nullptr)
        primeSamples = juce::jlimit (0, juce::jmax (0, maxLookAhead()), primeSamples);

    // the blocks that played silent while the input was still being
    // buffered: skip their share of the source so the clip stays on time
    if (deferredOutputSamples > 0)
    {
        auto skip = static_cast<int> (std::lround (deferredOutputSamples * ratio));
        deferredOutputSamples = 0;

        while (skip > 0)
        {
            const auto chunk = juce::jmin (skip, inputScratch.getNumSamples());
            pullInput (*input, chunk);
            skip -= chunk;
        }
    }

    pullInput (*input, primeSamples);
    backend->prime (inputScratch.getArrayOfReadPointers(), primeSamples, ratio);
    ++primeCount;
}

void StretchAudioSource::getNextAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    if (! enabled.load())
    {
        input->getNextAudioBlock (info);
        return;
    }

    if (! prepared)
    {
        info.clearActiveBufferRegion();
        return;
    }

    const auto ratio = speedRatio.load();

    if (needsPriming.load())
    {
        // the prime's look-ahead plus this block's input must be there
        if (inputReady != nullptr)
        {
            auto wanted = backend->primeInputLength (ratio)
                          + static_cast<int> (std::ceil (info.numSamples * ratio)) + 64;
            if (maxLookAhead != nullptr)
                wanted = juce::jmin (wanted, juce::jmax (0, maxLookAhead()));

            if (! inputReady (wanted))
            {
                info.clearActiveBufferRegion();
                deferredOutputSamples += info.numSamples;
                return;
            }
        }

        needsPriming.store (false);
        prime (ratio);
    }

    const auto inputSamples = juce::jlimit (0, inputScratch.getNumSamples(),
                                            backend->inputForOutput (info.numSamples, ratio));
    pullInput (*input, inputSamples);

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
