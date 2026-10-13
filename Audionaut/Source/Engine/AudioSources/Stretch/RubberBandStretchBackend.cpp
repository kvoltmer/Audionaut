//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "RubberBandStretchBackend.h"

namespace audium {

namespace {

int ceilToInt (double value)
{
    return static_cast<int> (std::ceil (value));
}

} // namespace

void RubberBandStretchBackend::prepare (int numChannels_, double sampleRate, int maxBlockSize,
                                        double minSpeedRatio, double maxSpeedRatio)
{
    using RubberBand::RubberBandStretcher;

    numChannels = numChannels_;
    blockSize = maxBlockSize;
    headroom = maxBlockSize;

    stretcher = std::make_unique<RubberBandStretcher> (static_cast<size_t> (sampleRate),
                                                       static_cast<size_t> (numChannels),
                                                       RubberBandStretcher::OptionProcessRealTime
                                                       | RubberBandStretcher::OptionEngineFiner
                                                       | RubberBandStretcher::OptionThreadingNever);

    // The library's window geometry (we never pitch-shift, so these do not
    // move with the ratio): a fresh stretcher wants one window of input
    // before it renders, and the real-time alignment recipe pads and trims
    // half a window each.
    windowSize = static_cast<int> (stretcher->getSamplesRequired());
    startPad   = static_cast<int> (stretcher->getPreferredStartPad());
    startDelay = static_cast<int> (stretcher->getStartDelay());

    // Feeds go in chunks of one window, drained in between, so the library's
    // own in/out buffers (2x / 8x this) stay small.
    maxProcessSize = juce::jmax (windowSize, 1024);
    stretcher->setMaxProcessSize (static_cast<size_t> (maxProcessSize));

    // The biggest single pull the node makes: a prime at the fastest ratio,
    // or one block's worth at the fastest ratio, or the library's fill.
    maxInput = juce::jmax (primeInputLength (maxSpeedRatio),
                           ceilToInt ((blockSize + headroom + hopSlack()) * maxSpeedRatio),
                           windowSize);

    // The most the FIFO ever holds: what a prime at the slowest ratio leaves
    // after the trim, or a short FIFO topped up by a full window at the
    // slowest ratio - plus one window of output for the library's hop
    // granularity.
    const auto primeOutput = ceilToInt ((startPad + primeInputLength (minSpeedRatio)) / minSpeedRatio) - startDelay;
    const auto windowOutput = ceilToInt (windowSize / minSpeedRatio);
    fifo.prepare (numChannels, juce::jmax (primeOutput, blockSize + headroom + hopSlack() + windowOutput) + windowOutput);

    retrieveScratch.setSize (numChannels, maxProcessSize);
    zeros.setSize (numChannels, juce::jmin (startPad, maxProcessSize));
    zeros.clear();

    currentRatio = 0.0;
    pendingDiscard = 0;
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

int RubberBandStretchBackend::primeInputLength (double ratio) const noexcept
{
    // The library renders one output hop per frame, and only while it holds
    // a full window of input: after the pad it needs (windowSize - startPad)
    // of material for the first frame, then ratio input per output sample.
    // After the start delay is trimmed the FIFO should hold one block, the
    // headroom and one hop of slack for the frame granularity.
    const auto output = startDelay + blockSize + headroom + hopSlack();
    return juce::jmax (1, windowSize - startPad + ceilToInt (output * ratio));
}

int RubberBandStretchBackend::hopSlack() const noexcept
{
    // an upper bound on the library's output hop at any rate (its preferred
    // maximum is sampleRate / 128, the window is at least sampleRate / 16)
    return windowSize / 8;
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
        drain();
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

        const auto wanted = juce::jmin (available, retrieveScratch.getNumSamples(), fifo.getFreeSpace() + pendingDiscard);
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
    beginPrime (ratio);
    feedPadding (startPad);
    feed (input, numInput);
}

void RubberBandStretchBackend::beginPrime (double ratio)
{
    stretcher->reset();
    fifo.clear();
    currentRatio = 0.0;
    applyRatio (ratio);

    // the library's alignment recipe for real-time mode
    pendingDiscard = startDelay;
}

void RubberBandStretchBackend::feedPadding (int numZeros)
{
    for (int done = 0; done < numZeros;)
    {
        const auto chunk = juce::jmin (zeros.getNumSamples(), numZeros - done);
        stretcher->process (zeros.getArrayOfReadPointers(), static_cast<size_t> (chunk), false);
        done += chunk;
    }
}

int RubberBandStretchBackend::inputForOutput (int numOutput, double ratio)
{
    // aim one block plus one hop ahead: the library releases output a hop
    // at a time, so an exact ask can fall short by up to that much
    const auto needed = numOutput + headroom + hopSlack() - fifo.getNumStored();
    auto wanted = needed > 0 ? ceilToInt (needed * ratio) : 0;

    // short of this block: at least what the library needs to produce
    // anything at all
    if (fifo.getNumStored() < numOutput)
        wanted = juce::jmax (wanted, static_cast<int> (stretcher->getSamplesRequired()));

    return juce::jmin (wanted, maxInput);
}

void RubberBandStretchBackend::process (const float* const* input, int numInput,
                                        float* const* output, int numOutput, double ratio)
{
    applyRatio (ratio);

    if (numInput > 0)
        feed (input, numInput);

    fifo.pop (output, numOutput);
}

} // namespace audium
