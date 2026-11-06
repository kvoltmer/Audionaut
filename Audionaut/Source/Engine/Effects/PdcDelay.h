//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <algorithm>
#include <vector>

#include <JuceHeader.h>

namespace audium
{

/**
 * A fixed delay of whole samples on a few channels: what latency
 * compensation inserts where two signals of different latency meet, so the
 * one that is ahead waits for the other (see MixPlan::Latency). Nothing
 * about it moves - no interpolation, no ramp - a block goes in and comes
 * out `delaySamples` later, sample for sample.
 *
 * Sized once, on the message thread, for the longest block the renderer
 * will hand it; process() then allocates nothing and takes any shorter
 * block. One render thread at a time uses an instance (the plan keeps every
 * delay on the job that owns its signal), and consecutive plans may share
 * an instance so the signal in flight survives a rebuild (see
 * MixerPlanner's carry-over).
 */
class PdcDelay
{
public:
    PdcDelay (int numChannels_, int delaySamples_, int maxBlockSize_) :
        numChannels (std::max (1, numChannels_)),
        delaySamples (std::max (0, delaySamples_)),
        maxBlockSize (std::max (1, maxBlockSize_)),
        capacity (delaySamples + maxBlockSize),
        ring (static_cast<size_t> (numChannels) * static_cast<size_t> (capacity), 0.0f)
    {
    }

    int getNumChannels() const noexcept { return numChannels; }
    int getDelaySamples() const noexcept { return delaySamples; }
    int getMaxBlockSize() const noexcept { return maxBlockSize; }

    /// Whether this instance can stand in for one built with these figures (the carry-over's question).
    bool matches (int numChannels_, int delaySamples_, int maxBlockSize_) const noexcept
    {
        return numChannels == numChannels_ && delaySamples == delaySamples_ && maxBlockSize == maxBlockSize_;
    }

    /// Render thread, nothing else using the instance: the line holds silence again.
    void reset() noexcept
    {
        std::fill (ring.begin(), ring.end(), 0.0f);
        writePosition = 0;
    }

    /**
     * Render thread: delays the block in place. Every channel of the block
     * up to the prepared count is delayed; a block of more channels keeps
     * the rest as they are. The block must not be longer than prepared.
     */
    void process (juce::dsp::AudioBlock<float>& block) noexcept
    {
        if (delaySamples == 0)
            return;

        const auto numSamples = static_cast<int> (block.getNumSamples());
        jassert (numSamples <= maxBlockSize);
        if (numSamples > maxBlockSize)
            return;   // longer than the ring was sized for: left as it is rather than written past the ring
        const auto channels = std::min (numChannels, static_cast<int> (block.getNumChannels()));

        for (auto c = 0; c < channels; ++c)
        {
            auto* line = ring.data() + static_cast<size_t> (c) * static_cast<size_t> (capacity);
            auto* samples = block.getChannelPointer (static_cast<size_t> (c));

            // the block goes into the ring at the write position, then comes
            // back out from `delaySamples` behind it; the ring is long enough
            // that the read never catches up with what is not yet written,
            // and where the read window reaches into the block just written
            // it finds exactly the samples that belong there
            copyIntoRing (line, writePosition, samples, numSamples);
            copyOutOfRing (samples, line, wrapped (writePosition - delaySamples), numSamples);
        }

        writePosition = wrapped (writePosition + numSamples);
    }

private:
    int wrapped (int position) const noexcept
    {
        position %= capacity;
        return position < 0 ? position + capacity : position;
    }

    /// `count` samples from `source` into the ring at `position`, in one or two pieces around the end.
    void copyIntoRing (float* line, int position, const float* source, int count) const noexcept
    {
        const auto first = std::min (count, capacity - position);
        juce::FloatVectorOperations::copy (line + position, source, first);
        if (first < count)
            juce::FloatVectorOperations::copy (line, source + first, count - first);
    }

    /// `count` samples out of the ring from `position` into `destination`, in one or two pieces around the end.
    void copyOutOfRing (float* destination, const float* line, int position, int count) const noexcept
    {
        const auto first = std::min (count, capacity - position);
        juce::FloatVectorOperations::copy (destination, line + position, first);
        if (first < count)
            juce::FloatVectorOperations::copy (destination + first, line, count - first);
    }

    const int numChannels;
    const int delaySamples;
    const int maxBlockSize;
    const int capacity;              ///< samples per channel: the delay plus one block, so a block never overwrites what it still has to read
    std::vector<float> ring;         ///< numChannels rows of capacity samples
    int writePosition = 0;           ///< where the next block goes, the same for every channel

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PdcDelay)
};

} // namespace audium
