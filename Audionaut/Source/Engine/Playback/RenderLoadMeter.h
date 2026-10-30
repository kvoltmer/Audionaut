//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include <atomic>

namespace audium
{

/**
 * How much of the block period each render thread spends inside jobs, and
 * how much of it the calling thread spends waiting for the others.
 *
 * The pool feeds it once per stage: beginStage() on the calling thread,
 * addBusy() from every thread that ran jobs, endStage() on the calling
 * thread after the barrier, when every addBusy() of the stage has landed.
 * The readings are smoothed like DspLoadMeter's and polled from the message
 * thread; 1.0 is the whole block period.
 *
 * Slot 0 is the calling thread, 1.. the workers.
 */
class RenderLoadMeter
{
public:
    RenderLoadMeter() = default;

    /// The calling thread and up to eight workers.
    static constexpr int maxThreadSlots = 9;

    /// Zeroes the readings and sets the period they are measured against. Not while a stage is open.
    void reset (int blockSize, double sampleRate) noexcept
    {
        const auto blockSeconds = sampleRate > 0.0 ? blockSize / sampleRate : 0.0;
        blockTicks = static_cast<juce::int64> (blockSeconds * static_cast<double> (juce::Time::getHighResolutionTicksPerSecond()));

        for (auto& load : threadLoad)
            load.store (0.0f, std::memory_order_relaxed);
        spinLoad.store (0.0f, std::memory_order_relaxed);
    }

    /// The period one stage is measured against, in high-resolution ticks.
    juce::int64 getBlockTicks() const noexcept { return blockTicks; }

    /// Calling thread, before the stage is published.
    void beginStage() noexcept
    {
        for (auto& ticks : stageBusyTicks)
            ticks.store (0, std::memory_order_relaxed);
    }

    /// The thread in `slot`, once it has run its jobs of the stage.
    void addBusy (int slot, juce::int64 ticks) noexcept
    {
        if (ticks > 0)
            stageBusyTicks[slot].fetch_add (ticks, std::memory_order_relaxed);
    }

    /// Calling thread, after the barrier: folds the stage into the readings.
    void endStage (juce::int64 spinTicks) noexcept
    {
        if (blockTicks <= 0)
            return;

        for (auto i = 0; i < maxThreadSlots; ++i)
            smooth (threadLoad[i], toLoad (stageBusyTicks[i].load (std::memory_order_relaxed)));
        smooth (spinLoad, toLoad (spinTicks));
    }

    /// Smoothed share of the period the thread in `slot` spent inside jobs; 0 for a slot out of range.
    float getLoad (int slot) const noexcept
    {
        return slot >= 0 && slot < maxThreadSlots ? threadLoad[slot].load (std::memory_order_relaxed) : 0.0f;
    }

    /// Smoothed share of the period the calling thread spent waiting for the workers.
    float getSpinLoad() const noexcept { return spinLoad.load (std::memory_order_relaxed); }

private:
    static constexpr float smoothingCoefficient = 0.2f;   // as DspLoadMeter

    float toLoad (juce::int64 ticks) const noexcept
    {
        return static_cast<float> (static_cast<double> (ticks) / static_cast<double> (blockTicks));
    }

    static void smooth (std::atomic<float>& reading, float value) noexcept
    {
        const auto previous = reading.load (std::memory_order_relaxed);
        reading.store (previous + smoothingCoefficient * (value - previous), std::memory_order_relaxed);
    }

    juce::int64 blockTicks = 0;
    std::atomic<juce::int64> stageBusyTicks[maxThreadSlots] = {};
    std::atomic<float> threadLoad[maxThreadSlots] = {};
    std::atomic<float> spinLoad { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RenderLoadMeter)
};

} // namespace audium
