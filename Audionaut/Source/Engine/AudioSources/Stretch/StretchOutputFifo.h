//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

namespace audium {

/**
 * A per-channel ring of rendered samples between the stretcher and the
 * node: Rubber Band hands output back in its own chunking, the node wants
 * exactly numOutput per block. Sized once in prepare(), allocation-free
 * afterwards.
 */
class StretchOutputFifo
{
public:
    void prepare (int numChannels, int capacity)
    {
        buffer.setSize (numChannels, capacity);
        buffer.clear();
        readIndex = 0;
        stored = 0;
        underruns = 0;
        overflows = 0;
    }

    void clear() noexcept
    {
        readIndex = 0;
        stored = 0;
    }

    int getNumStored() const noexcept    { return stored; }
    int getFreeSpace() const noexcept    { return buffer.getNumSamples() - stored; }
    int getCapacity() const noexcept     { return buffer.getNumSamples(); }
    int getUnderruns() const noexcept    { return underruns; }
    int getOverflows() const noexcept    { return overflows; }

    /// Appends @p numSamples per channel; what does not fit is dropped and
    /// counted as an overflow (the capacity is sized so it never happens).
    void push (const float* const* input, int numSamples) noexcept
    {
        const auto capacity = buffer.getNumSamples();
        const auto accepted = juce::jmin (numSamples, getFreeSpace());
        if (accepted < numSamples)
            ++overflows;

        const auto write = (readIndex + stored) % capacity;
        const auto first = juce::jmin (accepted, capacity - write);

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            buffer.copyFrom (channel, write, input[channel], first);
            if (accepted > first)
                buffer.copyFrom (channel, 0, input[channel] + first, accepted - first);
        }

        stored += accepted;
    }

    /// Takes @p numSamples per channel; short of that, the rest is silence
    /// and an underrun is counted.
    void pop (float* const* output, int numSamples) noexcept
    {
        const auto capacity = buffer.getNumSamples();
        const auto available = juce::jmin (numSamples, stored);
        const auto first = juce::jmin (available, capacity - readIndex);

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const auto* src = buffer.getReadPointer (channel);
            juce::FloatVectorOperations::copy (output[channel], src + readIndex, first);
            if (available > first)
                juce::FloatVectorOperations::copy (output[channel] + first, src, available - first);
            if (available < numSamples)
                juce::FloatVectorOperations::clear (output[channel] + available, numSamples - available);
        }

        if (available < numSamples)
            ++underruns;

        readIndex = (readIndex + available) % capacity;
        stored -= available;
    }

    /// Drops @p numSamples from the front (alignment trimming).
    int discard (int numSamples) noexcept
    {
        const auto dropped = juce::jmin (numSamples, stored);
        readIndex = (readIndex + dropped) % buffer.getNumSamples();
        stored -= dropped;
        return dropped;
    }

private:
    juce::AudioBuffer<float> buffer;
    int readIndex = 0;
    int stored = 0;
    int underruns = 0;
    int overflows = 0;
};

} // namespace audium
