//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

namespace audium {

/**
 * A per-channel ring of rendered samples for the push/pull stretchers
 * (Rubber Band, SoundTouch, Bungee): they hand output back in their own
 * chunking, the node wants exactly numOutput per block. Sized once in
 * prepare(), allocation-free afterwards.
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

    /// Appends @p numSamples per channel (dropping what does not fit).
    void push (const float* const* input, int numSamples) noexcept
    {
        const auto capacity = buffer.getNumSamples();
        numSamples = juce::jmin (numSamples, getFreeSpace());

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            auto* dest = buffer.getWritePointer (channel);
            auto write = (readIndex + stored) % capacity;

            for (int i = 0; i < numSamples; ++i)
            {
                dest[write] = input[channel][i];
                write = (write + 1) % capacity;
            }
        }

        stored += numSamples;
    }

    /// Takes @p numSamples per channel; short of that, the rest is silence
    /// and an underrun is counted.
    void pop (float* const* output, int numSamples) noexcept
    {
        const auto capacity = buffer.getNumSamples();
        const auto available = juce::jmin (numSamples, stored);

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const auto* src = buffer.getReadPointer (channel);
            auto read = readIndex;

            for (int i = 0; i < available; ++i)
            {
                output[channel][i] = src[read];
                read = (read + 1) % capacity;
            }

            for (int i = available; i < numSamples; ++i)
                output[channel][i] = 0.0f;
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
};

} // namespace audium
