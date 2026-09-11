//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "SignalsmithStretchBackend.h"

namespace audium {

void SignalsmithStretchBackend::prepare (int numChannels, double sampleRate, int, double maxSpeedRatio)
{
    stretch.presetDefault (numChannels, static_cast<float> (sampleRate));

    // Pre-warm the library's internal temp buffers off the audio thread, so
    // the first real prime and process allocate nothing.
    const auto maxPrime = stretch.outputSeekLength (static_cast<float> (maxSpeedRatio));
    warmup.setSize (numChannels, juce::jmax (1, maxPrime));
    warmup.clear();
    stretch.outputSeek (warmup.getArrayOfWritePointers(), maxPrime);
    stretch.reset();

    inputRemainder = 0.0;
}

int SignalsmithStretchBackend::maxInputLength (int maxBlockSize, double maxSpeedRatio) const
{
    // the biggest single pull: a priming read at the maximum speed, or one
    // block's worth of input at the maximum speed
    const auto maxPrime = stretch.outputSeekLength (static_cast<float> (maxSpeedRatio));
    const auto maxBlockPull = static_cast<int> (std::ceil (maxBlockSize * maxSpeedRatio)) + 2;
    return juce::jmax (maxPrime, maxBlockPull);
}

int SignalsmithStretchBackend::primeInputLength (double ratio) const
{
    return stretch.outputSeekLength (static_cast<float> (ratio));
}

void SignalsmithStretchBackend::prime (const float* const* input, int numInput, double)
{
    // outputSeek resets, seeks past the analysis latency and discards the
    // pre-roll internally: the next process() output starts exactly at
    // input[0].
    stretch.outputSeek (input, numInput);
    inputRemainder = 0.0;
}

int SignalsmithStretchBackend::inputForOutput (int numOutput, double ratio)
{
    // consume ratio input samples per output sample, carrying the fraction
    const auto wanted = static_cast<double> (numOutput) * ratio + inputRemainder;
    const auto inputSamples = static_cast<int> (wanted);
    inputRemainder = wanted - static_cast<double> (inputSamples);
    return inputSamples;
}

void SignalsmithStretchBackend::process (const float* const* input, int numInput,
                                         float* const* output, int numOutput, double)
{
    stretch.process (input, numInput, output, numOutput);
}

} // namespace audium
