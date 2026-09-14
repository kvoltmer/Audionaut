//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "SoundTouchStretchBackend.h"

#if STRETCH_SOUNDTOUCH_ENABLED

namespace audium {

namespace {

constexpr int drainChunk = 4096;

} // namespace

void SoundTouchStretchBackend::prepare (int numChannels_, double sampleRate, int maxBlockSize, double maxSpeedRatio)
{
    numChannels = numChannels_;
    headroom = maxBlockSize;
    maxInput = maxInputLength (maxBlockSize, maxSpeedRatio);

    for (auto* instance : { &soundTouch, &calibrator })
    {
        instance->setChannels (static_cast<uint> (numChannels));
        instance->setSampleRate (static_cast<uint> (sampleRate));
        instance->setSetting (SETTING_USE_QUICKSEEK, 0);
        instance->setSetting (SETTING_USE_AA_FILTER, 0);   // tempo only, no rate change
    }
    calibratedRatio = 0.0;
    pendingDiscard = 0;

    fifo.prepare (numChannels, juce::jmax (4 * maxInput, 16 * maxBlockSize));
    interleavedIn.assign (static_cast<size_t> (maxInput * numChannels), 0.0f);
    interleavedOut.assign (static_cast<size_t> (drainChunk * numChannels), 0.0f);
    deinterleaved.setSize (numChannels, drainChunk);

    currentRatio = 0.0;
}

int SoundTouchStretchBackend::maxInputLength (int maxBlockSize, double maxSpeedRatio) const
{
    // the initial latency at the fastest speed (a few thousand frames)
    // plus two blocks of headroom input
    return static_cast<int> (std::ceil (2.0 * maxBlockSize * maxSpeedRatio)) + 16384;
}

void SoundTouchStretchBackend::applyRatio (double ratio)
{
    if (ratio != currentRatio)
    {
        // SoundTouch's tempo is input per output - the same convention
        soundTouch.setTempo (ratio);
        currentRatio = ratio;
    }
}

int SoundTouchStretchBackend::primeInputLength (double ratio) const
{
    auto* self = const_cast<SoundTouchStretchBackend*> (this);
    self->applyRatio (ratio);
    const auto latency = self->soundTouch.getSetting (SETTING_INITIAL_LATENCY);
    const auto wanted = latency + static_cast<int> (std::ceil (headroom * ratio)) + 512;
    return juce::jlimit (1, maxInput, wanted);
}

void SoundTouchStretchBackend::feed (const float* const* input, int numInput)
{
    for (int done = 0; done < numInput;)
    {
        const auto chunk = juce::jmin (maxInput, numInput - done);

        for (int i = 0; i < chunk; ++i)
            for (int channel = 0; channel < numChannels; ++channel)
                interleavedIn[static_cast<size_t> (i * numChannels + channel)] = input[channel][done + i];

        soundTouch.putSamples (interleavedIn.data(), static_cast<uint> (chunk));
        done += chunk;
    }
}

void SoundTouchStretchBackend::feedSilence (int numInput)
{
    std::fill (interleavedIn.begin(), interleavedIn.end(), 0.0f);

    for (int done = 0; done < numInput;)
    {
        const auto chunk = juce::jmin (maxInput, numInput - done);
        soundTouch.putSamples (interleavedIn.data(), static_cast<uint> (chunk));
        done += chunk;
    }
}

void SoundTouchStretchBackend::drain()
{
    for (;;)
    {
        const auto available = static_cast<int> (soundTouch.numSamples());
        const auto wanted = juce::jmin (available, drainChunk, fifo.getFreeSpace() + pendingDiscard);
        if (wanted <= 0)
            break;

        const auto got = static_cast<int> (soundTouch.receiveSamples (interleavedOut.data(),
                                                                       static_cast<uint> (wanted)));
        if (got <= 0)
            break;

        const auto trim = juce::jmin (pendingDiscard, got);
        pendingDiscard -= trim;

        for (int i = trim; i < got; ++i)
            for (int channel = 0; channel < numChannels; ++channel)
                deinterleaved.setSample (channel, i - trim, interleavedOut[static_cast<size_t> (i * numChannels + channel)]);

        if (got > trim)
            fifo.push (deinterleaved.getArrayOfReadPointers(), got - trim);
    }
}

int SoundTouchStretchBackend::measureStartOffset (double ratio)
{
    if (ratio == calibratedRatio)
        return calibratedOffset;

    // a step at input frame stepAt: the first non-zero output frame k0 maps
    // to input stepAt, so output frame 0 maps to stepAt - k0 * ratio
    calibrator.clear();
    calibrator.setTempo (ratio);

    const auto latency = calibrator.getSetting (SETTING_INITIAL_LATENCY);
    const auto stepAt = juce::jmax (4096, latency);
    const auto total = juce::jmin (maxInput, stepAt * 2 + latency + 4096);

    for (int i = 0; i < total; ++i)
        for (int channel = 0; channel < numChannels; ++channel)
            interleavedIn[static_cast<size_t> (i * numChannels + channel)] = i >= stepAt ? 1.0f : 0.0f;
    calibrator.putSamples (interleavedIn.data(), static_cast<uint> (total));

    auto firstNonZero = -1;
    for (int base = 0; firstNonZero < 0;)
    {
        const auto got = static_cast<int> (calibrator.receiveSamples (interleavedOut.data(),
                                                                       static_cast<uint> (drainChunk)));
        if (got <= 0)
            break;

        for (int i = 0; i < got && firstNonZero < 0; ++i)
            if (std::abs (interleavedOut[static_cast<size_t> (i * numChannels)]) > 0.5f)
                firstNonZero = base + i;
        base += got;
    }

    calibratedRatio = ratio;
    calibratedOffset = firstNonZero >= 0
                           ? static_cast<int> (std::lround (stepAt - firstNonZero * ratio))
                           : 0;
    return calibratedOffset;
}

void SoundTouchStretchBackend::prime (const float* const* input, int numInput, double ratio)
{
    soundTouch.clear();
    fifo.clear();
    currentRatio = 0.0;
    applyRatio (ratio);

    // line output frame 0 up with input[0]: pad the input the stretcher
    // would skip, or trim the output it emits ahead of the start
    const auto offset = measureStartOffset (ratio);
    pendingDiscard = offset < 0 ? static_cast<int> (std::lround (-offset / ratio)) : 0;
    if (offset > 0)
        feedSilence (offset);

    feed (input, numInput);
    drain();
}

int SoundTouchStretchBackend::inputForOutput (int numOutput, double ratio)
{
    const auto needed = numOutput + headroom - fifo.getNumStored();
    auto wanted = needed > 0 ? static_cast<int> (std::ceil (needed * ratio)) : 0;

    // short of this block: at least a nominal input sequence, or nothing
    // comes out
    if (fifo.getNumStored() < numOutput)
        wanted = juce::jmax (wanted, soundTouch.getSetting (SETTING_NOMINAL_INPUT_SEQUENCE));

    return juce::jmin (wanted, maxInput);
}

void SoundTouchStretchBackend::process (const float* const* input, int numInput,
                                        float* const* output, int numOutput, double ratio)
{
    applyRatio (ratio);

    if (numInput > 0)
        feed (input, numInput);

    drain();
    fifo.pop (output, numOutput);
}

} // namespace audium

#endif // STRETCH_SOUNDTOUCH_ENABLED
