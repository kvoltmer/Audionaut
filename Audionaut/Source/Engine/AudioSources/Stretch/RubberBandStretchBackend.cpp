//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "RubberBandStretchBackend.h"

#if STRETCH_RUBBERBAND_ENABLED

namespace audium {

namespace {

// Rubber Band's real-time mode wants generous input for the finer engine:
// a fixed per-call ceiling keeps the node's scratch and the library's
// process size in step.
constexpr int inputCeiling = 65536;
constexpr int retrieveChunk = 4096;

} // namespace

void RubberBandStretchBackend::prepare (int numChannels_, double sampleRate, int maxBlockSize, double)
{
    using RubberBand::RubberBandStretcher;

    numChannels = numChannels_;
    headroom = maxBlockSize;
    maxProcessSize = inputCeiling;

    stretcher = std::make_unique<RubberBandStretcher> (static_cast<size_t> (sampleRate),
                                                       static_cast<size_t> (numChannels),
                                                       RubberBandStretcher::OptionProcessRealTime
                                                       | RubberBandStretcher::OptionEngineFiner
                                                       | RubberBandStretcher::OptionThreadingNever);
    stretcher->setMaxProcessSize (static_cast<size_t> (maxProcessSize));

    // the finer engine renders in bursts; room for a generous one plus the
    // alignment trim
    fifo.prepare (numChannels, juce::jmax (4 * inputCeiling, 16 * maxBlockSize));
    retrieveScratch.setSize (numChannels, retrieveChunk);
    zeros.setSize (numChannels, maxProcessSize);
    zeros.clear();

    currentRatio = 0.0;
    pendingDiscard = 0;
}

int RubberBandStretchBackend::maxInputLength (int, double) const
{
    return inputCeiling;
}

void RubberBandStretchBackend::applyRatio (double ratio)
{
    if (ratio != currentRatio)
    {
        // speed ratio = input per output; Rubber Band's time ratio is the inverse
        stretcher->setTimeRatio (1.0 / ratio);
        currentRatio = ratio;
    }
}

int RubberBandStretchBackend::primeInputLength (double ratio) const
{
    // enough to cover the start pad's delay, the library's own look-ahead
    // and one block of headroom
    const_cast<RubberBandStretchBackend*> (this)->applyRatio (ratio);
    const auto delay = static_cast<double> (stretcher->getStartDelay());
    const auto required = static_cast<int> (stretcher->getSamplesRequired());
    const auto wanted = static_cast<int> (std::ceil ((delay + headroom) * ratio)) + required + 1024;
    return juce::jlimit (1, inputCeiling, wanted);
}

void RubberBandStretchBackend::feed (const float* const* input, int numInput)
{
    for (int done = 0; done < numInput;)
    {
        const auto chunk = juce::jmin (maxProcessSize, numInput - done);
        const float* pointers[64];
        jassert (numChannels <= 64);
        for (int channel = 0; channel < numChannels; ++channel)
            pointers[channel] = input[channel] + done;

        stretcher->process (pointers, static_cast<size_t> (chunk), false);
        done += chunk;
    }
}

void RubberBandStretchBackend::drain()
{
    for (;;)
    {
        const auto available = stretcher->available();
        if (available <= 0)
            break;

        const auto wanted = juce::jmin (available, retrieveChunk, fifo.getFreeSpace() + pendingDiscard);
        if (wanted <= 0)
            break;

        const auto got = static_cast<int> (stretcher->retrieve (retrieveScratch.getArrayOfWritePointers(),
                                                                static_cast<size_t> (wanted)));
        if (got <= 0)
            break;

        const auto trim = juce::jmin (pendingDiscard, got);
        pendingDiscard -= trim;

        if (got > trim)
        {
            const float* pointers[64];
            for (int channel = 0; channel < numChannels; ++channel)
                pointers[channel] = retrieveScratch.getReadPointer (channel) + trim;
            fifo.push (pointers, got - trim);
        }
    }
}

void RubberBandStretchBackend::prime (const float* const* input, int numInput, double ratio)
{
    stretcher->reset();
    fifo.clear();
    currentRatio = 0.0;
    applyRatio (ratio);

    // the library's alignment recipe for real-time mode
    const auto pad = static_cast<int> (stretcher->getPreferredStartPad());
    pendingDiscard = static_cast<int> (stretcher->getStartDelay());

    for (int done = 0; done < pad;)
    {
        const auto chunk = juce::jmin (zeros.getNumSamples(), pad - done);
        stretcher->process (zeros.getArrayOfReadPointers(), static_cast<size_t> (chunk), false);
        done += chunk;
    }

    feed (input, numInput);
    drain();
}

int RubberBandStretchBackend::inputForOutput (int numOutput, double ratio)
{
    const auto needed = numOutput + headroom - fifo.getNumStored();
    auto wanted = needed > 0 ? static_cast<int> (std::ceil (needed * ratio)) : 0;

    // short of this block: at least what the library needs to produce
    // anything at all
    if (fifo.getNumStored() < numOutput)
        wanted = juce::jmax (wanted, static_cast<int> (stretcher->getSamplesRequired()));

    return juce::jmin (wanted, inputCeiling);
}

void RubberBandStretchBackend::process (const float* const* input, int numInput,
                                        float* const* output, int numOutput, double ratio)
{
    applyRatio (ratio);

    if (numInput > 0)
        feed (input, numInput);

    drain();
    fifo.pop (output, numOutput);
}

} // namespace audium

#endif // STRETCH_RUBBERBAND_ENABLED
