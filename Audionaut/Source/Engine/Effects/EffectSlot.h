//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <atomic>
#include <memory>

#include "Effect.h"

namespace audium
{

/**
 * One position in an insert chain: the effect, its enabled state and the
 * click-free bypass. Slots are shared between the chain that owns the
 * position and every mix plan that references it, so a removed slot lives
 * on until the audio thread has let go of the last plan that had it.
 *
 * prepare() runs on the message thread while no live plan uses the slot:
 * the planner prepares a fresh slot before it enters a plan and replaces a
 * live one whose format changed. process() runs on whichever render thread
 * takes the job; the enabled flag is read once per block. Toggling it
 * crossfades the dry and the processed signal over about ten milliseconds.
 */
class EffectSlot
{
public:
    explicit EffectSlot (std::unique_ptr<Effect> effect_) :
        effect (std::move (effect_))
    {
        jassert (effect != nullptr);
    }

    Effect& getEffect() noexcept { return *effect; }
    const Effect& getEffect() const noexcept { return *effect; }

    bool isEnabled() const noexcept { return enabled.load (std::memory_order_relaxed); }
    void setEnabled (bool shouldBeEnabled) noexcept { enabled.store (shouldBeEnabled, std::memory_order_relaxed); }

    /// Message thread: sizes the effect and the bypass for this format.
    void prepare (double sampleRate_, int maxBlockSize_, int numChannels_)
    {
        sampleRate = sampleRate_;
        maxBlockSize = maxBlockSize_;
        numChannels = numChannels_;
        rampStep = static_cast<float> (1.0 / juce::jmax (1.0, sampleRate * rampSeconds));
        dry.setSize (numChannels, maxBlockSize);
        effect->prepare (sampleRate, maxBlockSize, numChannels);
        wet = isEnabled() ? 1.0f : 0.0f;
        prepared = true;
    }

    bool isPreparedFor (double sampleRate_, int maxBlockSize_, int numChannels_) const noexcept
    {
        return prepared && sampleRate == sampleRate_ && maxBlockSize >= maxBlockSize_ && numChannels == numChannels_;
    }

    bool isPrepared() const noexcept { return prepared; }
    int getNumChannels() const noexcept { return numChannels; }

    /// Whether a published plan has referenced this slot: then its format cannot change in place.
    bool isPublished() const noexcept { return published; }
    void markPublished() noexcept { published = true; }

    /// Render thread: the effect in place, or the crossfade into or out of it.
    void process (juce::dsp::AudioBlock<float>& block) noexcept
    {
        if (! prepared || static_cast<int> (block.getNumChannels()) != numChannels || static_cast<int> (block.getNumSamples()) > maxBlockSize)
            return;

        const auto target = isEnabled() ? 1.0f : 0.0f;

        if (wet == target)
        {
            if (target > 0.0f)
                effect->process (block);
            return;   // bypassed and settled: the effect sees nothing
        }

        // the effect comes back from a bypass with a clean slate, so the
        // first processed block is not a burst of stale state
        if (wet <= 0.0f && target > 0.0f)
            effect->reset();

        const auto numSamples = static_cast<int> (block.getNumSamples());
        juce::dsp::AudioBlock<float> dryBlock (dry);
        auto dryView = dryBlock.getSubBlock (0, static_cast<size_t> (numSamples));
        dryView.copyFrom (block);
        effect->process (block);

        for (auto i = 0; i < numSamples; ++i)
        {
            wet = target > wet ? juce::jmin (target, wet + rampStep) : juce::jmax (target, wet - rampStep);
            for (auto c = 0; c < numChannels; ++c)
            {
                auto* out = block.getChannelPointer (static_cast<size_t> (c));
                const auto* in = dryView.getChannelPointer (static_cast<size_t> (c));
                out[i] = in[i] + (out[i] - in[i]) * wet;
            }
        }
    }

private:
    static constexpr double rampSeconds = 0.01;

    std::unique_ptr<Effect> effect;
    std::atomic<bool> enabled { true };

    double sampleRate = 0.0;
    int maxBlockSize = 0;
    int numChannels = 0;
    bool prepared = false;
    bool published = false;

    juce::AudioBuffer<float> dry;   // the unprocessed block, for the crossfade
    float wet = 1.0f;               // render thread: 1 = the effect, 0 = bypassed
    float rampStep = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectSlot)
};

} // namespace audium
