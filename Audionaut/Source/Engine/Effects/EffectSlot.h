//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <atomic>
#include <memory>

#include "Effect.h"
#include "PdcDelay.h"

namespace audium
{

/**
 * One position in an insert chain: the effect, its enabled state and the
 * click-free bypass. Slots are shared between the chain that owns the
 * position and every mix plan that references it, so a removed slot lives
 * on until the audio thread has let go of the last plan that had it.
 *
 * prepare() runs on the message thread while nothing renders the slot:
 * before it enters its first plan, or again - same instance, new format -
 * when the device is stopped or bypassed, or once every plan that ran it
 * is gone (the planner sees to that). process() runs on whichever render
 * thread takes the job; the enabled flag is read once per block. Toggling
 * it crossfades the dry and the processed signal over about ten
 * milliseconds.
 *
 * A slot is as late as its effect says (getLatencySamples, read at
 * prepare), whether the effect runs or not: bypassed, the slot delays the
 * dry signal by the same amount, and the crossfade mixes the delayed dry
 * with the processed signal. So toggling an effect never shifts what comes
 * after it, and the planner's latency figures depend on the chain alone.
 * An effect whose latency follows a parameter (the compressor's look-ahead)
 * makes the slot stale when that changes: isPreparedFor() says no, and the
 * planner prepares it again, as after a format change, before it enters
 * the next plan (see InsertChain::setParameter, which asks for that plan).
 * Coming back from a bypass, a latent effect's first latency's worth of
 * output is the pre-roll of a signal it has not heard yet: the delayed dry
 * carries on for that long, and the crossfade starts once the effect's
 * output is the signal - aligned, and without a hole. So a latent effect
 * comes back its latency later than it was switched on, at the cost of no
 * extra work in the block that switches it.
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

    /// Message thread, nothing rendering the slot: sizes the effect and the bypass for this format, again if need be.
    void prepare (double sampleRate_, int maxBlockSize_, int numChannels_)
    {
        sampleRate = sampleRate_;
        maxBlockSize = maxBlockSize_;
        numChannels = numChannels_;
        rampStep = static_cast<float> (1.0 / juce::jmax (1.0, sampleRate * rampSeconds));
        dry.setSize (numChannels, maxBlockSize);
        effect->prepare (sampleRate, maxBlockSize, numChannels);
        latencySamples = std::max (0, effect->getLatencySamples());
        bypassDelay = latencySamples > 0 ? std::make_unique<PdcDelay> (numChannels, latencySamples, maxBlockSize) : nullptr;
        wet = isEnabled() ? 1.0f : 0.0f;
        returning = false;
        catchingUp = 0;
        prepared = true;
    }

    /// How many samples later the slot's output is than its input, enabled or bypassed; 0 before prepare().
    int getLatencySamples() const noexcept { return latencySamples; }

    /// Message thread: the effect's latency moved since prepare() (a parameter changed it); the slot needs preparing again.
    bool hasStaleLatency() const noexcept
    {
        return prepared && latencySamples != std::max (0, effect->getLatencySamples());
    }

    bool isPreparedFor (double sampleRate_, int maxBlockSize_, int numChannels_) const noexcept
    {
        return prepared && sampleRate == sampleRate_ && maxBlockSize == maxBlockSize_ && numChannels == numChannels_
            && ! hasStaleLatency();
    }

    bool isPrepared() const noexcept { return prepared; }
    int getNumChannels() const noexcept { return numChannels; }

    /**
     * Whoever renders next, while nothing else does (a bounce about to
     * start, with the device bypassed): the effect forgets what it has
     * processed and the bypass settles, so the render starts from rest
     * rather than from the tail of live playback.
     */
    void reset() noexcept
    {
        if (! prepared)
            return;
        effect->reset();
        if (bypassDelay != nullptr)
            bypassDelay->reset();
        wet = isEnabled() ? 1.0f : 0.0f;
        returning = false;
        catchingUp = 0;
    }

    /// Whether a plan the render thread may run references the slot (counted by MixPlan).
    bool isInAPlan() const noexcept { return planReferences > 0; }
    void addPlanReference() noexcept { ++planReferences; }
    void removePlanReference() noexcept { jassert (planReferences > 0); --planReferences; }

    /// Render thread: the effect in place, or the crossfade into or out of it.
    void process (juce::dsp::AudioBlock<float>& block) noexcept
    {
        if (! prepared || static_cast<int> (block.getNumChannels()) != numChannels || static_cast<int> (block.getNumSamples()) > maxBlockSize)
            return;

        const auto target = isEnabled() ? 1.0f : 0.0f;
        const auto numSamples = static_cast<int> (block.getNumSamples());

        if (target <= 0.0f)
        {
            // bypassed, caught up or not: the next return starts afresh
            returning = false;
            catchingUp = 0;
        }

        if (wet == target)
        {
            if (target > 0.0f)
            {
                // enabled and settled: the effect alone - the dry line keeps
                // running on the side, so a bypass finds the signal in it
                if (bypassDelay != nullptr)
                {
                    juce::dsp::AudioBlock<float> dryBlock (dry);
                    auto dryView = dryBlock.getSubBlock (0, static_cast<size_t> (numSamples));
                    dryView.copyFrom (block);
                    bypassDelay->process (dryView);
                }
                effect->process (block);
            }
            else if (bypassDelay != nullptr)
            {
                bypassDelay->process (block);   // bypassed and settled: as late as the effect would be, nothing else
            }
            return;   // bypassed and settled: the effect sees nothing
        }

        // the effect comes back from a bypass with a clean slate, so the
        // first processed block is not a burst of stale state. A latent one
        // has nothing to say for its first latencySamples - that output is
        // the pre-roll of a signal it has not heard yet - so the delayed dry
        // goes on alone until then, and the crossfade starts aligned
        if (wet <= 0.0f && target > 0.0f && ! returning)
        {
            returning = true;
            effect->reset();
            catchingUp = latencySamples;
        }

        juce::dsp::AudioBlock<float> dryBlock (dry);
        auto dryView = dryBlock.getSubBlock (0, static_cast<size_t> (numSamples));
        dryView.copyFrom (block);
        if (bypassDelay != nullptr)
            bypassDelay->process (dryView);   // the dry side as late as the wet one, so the crossfade mixes aligned signals
        effect->process (block);

        for (auto i = 0; i < numSamples; ++i)
        {
            if (catchingUp > 0)
            {
                --catchingUp;   // the dry alone while the effect catches up
                for (auto c = 0; c < numChannels; ++c)
                    block.getChannelPointer (static_cast<size_t> (c))[i] = dryView.getChannelPointer (static_cast<size_t> (c))[i];
                continue;
            }
            wet = target > wet ? juce::jmin (target, wet + rampStep) : juce::jmax (target, wet - rampStep);
            for (auto c = 0; c < numChannels; ++c)
            {
                auto* out = block.getChannelPointer (static_cast<size_t> (c));
                const auto* in = dryView.getChannelPointer (static_cast<size_t> (c));
                out[i] = in[i] + (out[i] - in[i]) * wet;
            }
        }
        if (wet == target)
            returning = false;
    }

private:
    static constexpr double rampSeconds = 0.01;

    std::unique_ptr<Effect> effect;
    std::atomic<bool> enabled { true };

    double sampleRate = 0.0;
    int maxBlockSize = 0;
    int numChannels = 0;
    bool prepared = false;
    int planReferences = 0;   // message thread: plans alive that point at this slot

    juce::AudioBuffer<float> dry;   // the unprocessed block, for the crossfade
    int latencySamples = 0;         // the effect's, as read at prepare(); hasStaleLatency() when it moved since
    std::unique_ptr<PdcDelay> bypassDelay;   // the dry signal's share of that latency; nullptr for a prompt effect
    bool returning = false;         // render thread: on the way back from a bypass, the reset done
    int catchingUp = 0;             // render thread: samples of pre-roll the effect still owes before the crossfade starts
    float wet = 1.0f;               // render thread: 1 = the effect, 0 = bypassed
    float rampStep = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectSlot)
};

} // namespace audium
