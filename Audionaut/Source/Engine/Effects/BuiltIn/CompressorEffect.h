//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <vector>

#include "Engine/Effects/Effect.h"

namespace audium
{

/**
 * Compressor: a feed-forward compressor (threshold, ratio, attack, release)
 * with an optional look-ahead, a make-up gain after it, and a reading of its
 * gain reduction for the editor's meter.
 *
 * The gain computer is the one of juce_dsp's Compressor - a peak envelope
 * through a ballistics filter, the gain from the ratio where the envelope is
 * over the threshold - written out here because that class keeps its gain
 * to itself. The reduction reported is the compressor's alone, before the
 * make-up gain.
 *
 * The look-ahead delays the signal, not the detector: the gain worked out
 * from a sample is applied to the sample that went in the look-ahead
 * earlier, so the attack is under way when a transient arrives. The delay
 * is the effect's latency, reported for compensation; a change of the
 * look-ahead changes it, and the slot is prepared again for the new figure
 * (see EffectSlot). The line is sized for the longest look-ahead at
 * prepare(), so a change allocates nothing.
 */
class CompressorEffect final : public Effect
{
public:
    enum Parameter { thresholdDb = 0, ratio, attackMs, releaseMs, lookaheadMs, makeupDb };

    static constexpr std::array<EffectParameterInfo, 6> parameterInfos {{
        { "threshold_db", "Threshold",  "dB", -60.0f,    0.0f,  -18.0f },
        { "ratio",        "Ratio",      ":1",   1.0f,   20.0f,    4.0f },
        { "attack_ms",    "Attack",     "ms",   0.1f,  200.0f,   10.0f },
        { "release_ms",   "Release",    "ms",   5.0f, 2000.0f,  100.0f },
        { "lookahead_ms", "Look-ahead", "ms",   0.0f,   20.0f,    0.0f, false, nullptr, nullptr, true },   // moves the latency
        { "makeup_db",    "Make-up",    "dB",   0.0f,   24.0f,    0.0f },
    }};

    static EffectTypeInfo typeInfo() { return { "builtin:compressor", "Compressor", "Dynamics", "A compressor with threshold, ratio, attack, release, look-ahead and make-up gain." }; }

    CompressorEffect() : Effect (typeInfo(), parameterInfos) {}

    void prepare (double sampleRate_, int maxBlockSize, int numChannels) override
    {
        sampleRate = sampleRate_;
        juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32> (maxBlockSize), static_cast<juce::uint32> (numChannels) };
        envelope.setLevelCalculationType (juce::dsp::BallisticsFilterLevelCalculationType::peak);
        envelope.prepare (spec);
        makeup.setRampDurationSeconds (0.01);
        makeup.prepare (spec);

        // one line per channel, a power of two long enough for the longest look-ahead
        maxLookaheadSamples = lookaheadSamplesFor (parameterInfos[lookaheadMs].max);
        auto size = 1;
        while (size <= maxLookaheadSamples)
            size <<= 1;
        mask = size - 1;
        lines.assign (static_cast<size_t> (numChannels), std::vector<float> (static_cast<size_t> (size), 0.0f));

        applyParameters();
        reset();
    }

    void release() override
    {
        lines.clear();
        mask = 0;
        maxLookaheadSamples = 0;
    }

    void reset() noexcept override
    {
        envelope.reset();
        makeup.reset();
        for (auto& line : lines)
            std::fill (line.begin(), line.end(), 0.0f);
        writeIndex = 0;
        gainReductionPeakDb.store (0.0f, std::memory_order_relaxed);
    }

    void process (juce::dsp::AudioBlock<float>& block) noexcept override
    {
        if (lines.empty())
            return;   // not prepared

        if (takeParameterChange())
            applyParameters();

        const auto numSamples = static_cast<int> (block.getNumSamples());
        auto leastGain = 1.0f;
        for (size_t channel = 0; channel < block.getNumChannels() && channel < lines.size(); ++channel)
        {
            auto* samples = block.getChannelPointer (channel);
            auto* line = lines[channel].data();
            auto write = writeIndex;
            for (auto i = 0; i < numSamples; ++i)
            {
                const auto in = samples[i];
                const auto level = envelope.processSample (static_cast<int> (channel), in);
                const auto gain = level < threshold ? 1.0f : std::pow (level * thresholdInverse, ratioInverse - 1.0f);
                leastGain = std::min (leastGain, gain);

                // the gain of this sample goes onto the one that went in the look-ahead earlier
                line[write] = in;
                samples[i] = gain * line[(write - lookaheadSamples) & mask];
                write = (write + 1) & mask;
            }
        }
        writeIndex = (writeIndex + numSamples) & mask;

        // the block's deepest reduction joins the peak the editor has not taken yet
        const auto reductionDb = -juce::Decibels::gainToDecibels (leastGain, -200.0f);
        auto peak = gainReductionPeakDb.load (std::memory_order_relaxed);
        while (reductionDb > peak && ! gainReductionPeakDb.compare_exchange_weak (peak, reductionDb, std::memory_order_relaxed)) {}

        juce::dsp::ProcessContextReplacing<float> context (block);
        makeup.process (context);
    }

    /// The look-ahead, in samples at the prepared rate; 0 before prepare().
    int getLatencySamples() const noexcept override
    {
        return lines.empty() ? 0 : std::min (lookaheadSamplesFor (getParameter (lookaheadMs)), maxLookaheadSamples);
    }

    bool hasGainReductionMeter() const noexcept override { return true; }

    float takeGainReductionDb() noexcept override
    {
        return gainReductionPeakDb.exchange (0.0f, std::memory_order_relaxed);
    }

private:
    int lookaheadSamplesFor (float ms) const noexcept
    {
        return juce::roundToInt (static_cast<double> (ms) * 0.001 * sampleRate);
    }

    void applyParameters() noexcept
    {
        threshold = juce::Decibels::decibelsToGain (getParameter (thresholdDb), -200.0f);
        thresholdInverse = 1.0f / threshold;
        ratioInverse = 1.0f / getParameter (ratio);
        envelope.setAttackTime (getParameter (attackMs));
        envelope.setReleaseTime (getParameter (releaseMs));
        lookaheadSamples = std::min (lookaheadSamplesFor (getParameter (lookaheadMs)), maxLookaheadSamples);
        makeup.setGainDecibels (getParameter (makeupDb));
    }

    double sampleRate = 0.0;
    juce::dsp::BallisticsFilter<float> envelope;
    float threshold = 1.0f, thresholdInverse = 1.0f, ratioInverse = 1.0f;   // render thread, from applyParameters()
    int lookaheadSamples = 0;                                               // render thread, from applyParameters()
    int maxLookaheadSamples = 0;
    int mask = 0;
    std::vector<std::vector<float>> lines;   ///< the signal waiting for its gain, one line per channel
    int writeIndex = 0;                      // render thread
    juce::dsp::Gain<float> makeup;
    std::atomic<float> gainReductionPeakDb { 0.0f };
};

} // namespace audium
