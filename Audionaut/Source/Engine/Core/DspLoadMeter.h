//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include <atomic>
#include <cstdint>

namespace audium {

/**
 * @brief Measures how much of its time budget each audio callback consumes.
 *
 * The audio thread brackets its work with begin()/end(); both are lock-free and
 * allocation-free. The message thread polls getLoad() (a smoothed value) and
 * takePeak() (the worst callback since the last poll) to drive a load meter.
 *
 * A value of 1.0 means the callback took exactly as long as the audio it
 * produced; anything above that is an overrun and will audibly drop out.
 */
class DspLoadMeter
{
public:
    DspLoadMeter() = default;

    /// Audio thread: call at the start of the callback.
    void begin() noexcept
    {
        startTicks = juce::Time::getHighResolutionTicks();
    }

    /// Audio thread: call at the end of the callback that rendered numSamples at sampleRate.
    void end (int numSamples, double sampleRate) noexcept
    {
        const auto elapsedTicks = juce::Time::getHighResolutionTicks() - startTicks;
        const auto elapsedSeconds = juce::Time::highResolutionTicksToSeconds (elapsedTicks);
        const auto budgetSeconds = sampleRate > 0.0 ? static_cast<double> (numSamples) / sampleRate : 0.0;
        record (elapsedSeconds, budgetSeconds);
    }

    /// Audio thread (or tests): record one callback that took elapsedSeconds out of budgetSeconds.
    void record (double elapsedSeconds, double budgetSeconds) noexcept
    {
        if (budgetSeconds <= 0.0 || elapsedSeconds < 0.0)
            return;

        const auto ratio = static_cast<float> (elapsedSeconds / budgetSeconds);

        // exponential average - the UI polls far slower than callbacks arrive
        const auto previous = smoothed.load (std::memory_order_relaxed);
        smoothed.store (previous + smoothingCoefficient * (ratio - previous), std::memory_order_relaxed);

        // running maximum, cleared by takePeak()
        auto currentPeak = peak.load (std::memory_order_relaxed);
        while (ratio > currentPeak
               && ! peak.compare_exchange_weak (currentPeak, ratio, std::memory_order_relaxed))
        {}

        if (ratio >= 1.0f)
            overruns.fetch_add (1, std::memory_order_relaxed);
    }

    /// Message thread: smoothed load, 1.0 == the whole callback budget.
    float getLoad() const noexcept
    {
        return smoothed.load (std::memory_order_relaxed);
    }

    /// Message thread: highest load since the previous call, then resets it to zero.
    float takePeak() noexcept
    {
        return peak.exchange (0.0f, std::memory_order_relaxed);
    }

    /// Number of callbacks that exceeded their budget since the last reset().
    std::uint32_t getOverrunCount() const noexcept
    {
        return overruns.load (std::memory_order_relaxed);
    }

    void reset() noexcept
    {
        smoothed.store (0.0f, std::memory_order_relaxed);
        peak.store (0.0f, std::memory_order_relaxed);
        overruns.store (0, std::memory_order_relaxed);
    }

private:
    static constexpr float smoothingCoefficient = 0.2f;

    juce::int64 startTicks = 0;
    std::atomic<float> smoothed { 0.0f };
    std::atomic<float> peak { 0.0f };
    std::atomic<std::uint32_t> overruns { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DspLoadMeter)
};

} // namespace audium
