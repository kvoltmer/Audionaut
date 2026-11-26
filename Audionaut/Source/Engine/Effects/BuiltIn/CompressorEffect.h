//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "Biquad.h"
#include "Engine/Effects/Effect.h"

namespace audium
{

/**
 * Compressor: a feed-forward compressor (threshold, ratio, knee, attack,
 * release) with a high-pass in its side chain, an optional look-ahead, a
 * make-up gain after it, and a reading of its gain reduction for the
 * editor's meter.
 *
 * The detector works on the reduction, in decibels: the level of each sample
 * goes through the static curve - nothing under the knee, the ratio's slope
 * over it, a parabola joining the two across the knee - and the reduction
 * that curve asks for is what attack and release smooth. So the two times
 * are time constants of the gain itself: after the release time 63 % of the
 * reduction is gone, however deep it was, and it fades out rather than
 * stopping where a level crosses the threshold.
 *
 * The level is each channel's mean square over some 10 ms, scaled so that a
 * sine reads its peak, or with RMS off the sample's own magnitude. The mean
 * square keeps the gain from riding the single waves of a low note, which is
 * heard as distortion; it also takes those 10 ms to rise, so an attack
 * shorter than that is not heard as one. The peak reading answers a
 * transient at once, which is what a short attack and a look-ahead want.
 *
 * The side chain's high-pass (12 dB per octave, switched on by its own
 * parameter) is in the detector's path only, never in the audio: it keeps a
 * kick or a bass from turning the whole signal down.
 *
 * A sample that is not a number, or over the detector's ceiling of 24 dB
 * above full scale, reads as that ceiling: it turns the gain down like a
 * loud sample, no more, and leaves nothing behind in the detector's state.
 * The sample itself goes on as it came; only the detector is kept from it.
 *
 * The channels are linked: each has its own level, the loudest of them goes
 * through the curve and every channel gets that gain, so a hit on one side
 * turns both down and the stereo image stays put. The reduction reported is
 * the compressor's alone, before the make-up gain.
 *
 * The look-ahead delays the signal, not the detector: the gain worked out
 * from a sample is applied to the sample that went in the look-ahead
 * earlier, so the attack is under way when a transient arrives. The delay
 * is the effect's latency, reported for compensation; a change of the
 * look-ahead changes it, and the slot is prepared again for the new figure
 * (see EffectSlot). Until then the delay stays at the prepared figure, the
 * one the running plan compensates: a new look-ahead is heard only from the
 * prepare() that comes with a plan built for it. The line is sized for the
 * longest look-ahead at prepare().
 */
class CompressorEffect final : public Effect
{
public:
    enum Parameter { thresholdDb = 0, ratio, kneeDb, attackMs, releaseMs, rms, sidechainOn, sidechainHz, lookaheadMs, makeupDb };

    static constexpr std::array<EffectParameterInfo, 10> parameterInfos {{
        { "threshold_db", "Threshold",  "dB", -60.0f,    0.0f,  -18.0f },
        { "ratio",        "Ratio",      ":1",   1.0f,   20.0f,    4.0f },
        { "knee_db",      "Knee",       "dB",   0.0f,   12.0f,    6.0f },
        { "attack_ms",    "Attack",     "ms",   0.1f,  200.0f,   10.0f },
        { "release_ms",   "Release",    "ms",   5.0f, 2000.0f,  100.0f },
        { "rms",          "RMS",        "",     0.0f,    1.0f,    1.0f, true },
        { "sidechain_on", "SC high-pass", "",   0.0f,    1.0f,    0.0f, true },
        { "sidechain_hz", "SC freq",    "Hz",  20.0f,  500.0f,  100.0f },
        { "lookahead_ms", "Look-ahead", "ms",   0.0f,   20.0f,    0.0f, false, nullptr, nullptr, true },   // moves the latency
        { "makeup_db",    "Make-up",    "dB",   0.0f,   24.0f,    0.0f },
    }};

    static EffectTypeInfo typeInfo() { return { "builtin:compressor", "Compressor", "Dynamics", "A compressor with threshold, ratio, knee, attack, release, RMS or peak detection, a side-chain high-pass, look-ahead and make-up gain." }; }

    CompressorEffect() : Effect (typeInfo(), parameterInfos) {}

    void prepare (double sampleRate_, int maxBlockSize, int numChannels) override
    {
        sampleRate = sampleRate_;
        juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32> (maxBlockSize), static_cast<juce::uint32> (numChannels) };
        makeup.setRampDurationSeconds (0.01);
        makeup.prepare (spec);
        meanSquareCoefficient = coefficientFor (meanSquareMs);
        sidechain.states.assign (static_cast<size_t> (numChannels), {});
        meanSquares.assign (static_cast<size_t> (numChannels), 0.0f);

        // one line per channel, a power of two long enough for the longest look-ahead
        maxLookaheadSamples = lookaheadSamplesFor (parameterInfos[lookaheadMs].max);
        auto size = 1;
        while (size <= maxLookaheadSamples)
            size <<= 1;
        mask = size - 1;
        lines.assign (static_cast<size_t> (numChannels), std::vector<float> (static_cast<size_t> (size), 0.0f));

        applyParameters();
        lookaheadSamples = std::min (lookaheadSamplesFor (getParameter (lookaheadMs)), maxLookaheadSamples);
        reset();
    }

    void release() override
    {
        lines.clear();
        sidechain.states.clear();
        meanSquares.clear();
        mask = 0;
        maxLookaheadSamples = 0;
    }

    void reset() noexcept override
    {
        reductionDb = 0.0f;
        std::fill (meanSquares.begin(), meanSquares.end(), 0.0f);
        sidechain.clearState();
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
        const auto numChannels = std::min (block.getNumChannels(), lines.size());
        auto deepestDb = 0.0f;
        for (auto i = 0; i < numSamples; ++i)
        {
            // linked: the loudest channel, as the side chain hears it, drives the one gain all of them get
            auto power = 0.0f;
            for (size_t channel = 0; channel < numChannels; ++channel)
            {
                // written so that a sample that is not a number fails the test too
                auto sample = static_cast<double> (block.getChannelPointer (channel)[i]);
                if (! (std::abs (sample) <= detectorCeiling))
                    sample = detectorCeiling;
                if (sidechain.active)
                    sample = sidechain.process (sidechain.states[channel], sample);

                // the channel's level as a power: the sample's own, or twice the mean square, where a sine reads its peak
                const auto own = static_cast<float> (sample * sample);
                auto& meanSquare = meanSquares[channel];
                meanSquare = own + meanSquareCoefficient * (meanSquare - own);
                power = std::max (power, rmsDetection ? 2.0f * meanSquare : own);
            }

            // the static curve: what reduction this level asks for
            auto wantedDb = 0.0f;
            if (power > kneeStartPower)
            {
                const auto overDb = std::max (10.0f * std::log10 (power) - threshold, -halfKnee);   // rounding may land a hair under the knee
                wantedDb = overDb >= halfKnee ? slope * overDb
                                              : slope * (overDb + halfKnee) * (overDb + halfKnee) / (4.0f * halfKnee);
            }

            // attack towards more reduction, release towards less
            reductionDb = wantedDb + (wantedDb > reductionDb ? attackCoefficient : releaseCoefficient) * (reductionDb - wantedDb);
            if (wantedDb == 0.0f && reductionDb < 1.0e-6f)
                reductionDb = 0.0f;   // released: unity exactly
            deepestDb = std::max (deepestDb, reductionDb);
            const auto gain = reductionDb > 0.0f ? std::exp (reductionDb * decibelsToLog) : 1.0f;

            // the gain of this sample goes onto the one that went in the look-ahead earlier
            for (size_t channel = 0; channel < numChannels; ++channel)
            {
                auto* samples = block.getChannelPointer (channel);
                auto* line = lines[channel].data();
                line[writeIndex] = samples[i];
                samples[i] = gain * line[(writeIndex - lookaheadSamples) & mask];
            }
            writeIndex = (writeIndex + 1) & mask;
        }

        // the block's deepest reduction joins the peak the editor has not taken yet
        auto peak = gainReductionPeakDb.load (std::memory_order_relaxed);
        while (deepestDb > peak && ! gainReductionPeakDb.compare_exchange_weak (peak, deepestDb, std::memory_order_relaxed)) {}

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
    static constexpr float meanSquareMs = 10.0f;                  ///< the time constant of the RMS reading
    static constexpr double detectorCeiling = 16.0;               ///< 24 dB over full scale: what the detector reads at most
    static constexpr float decibelsToLog = -0.11512925464970229f;   ///< -ln (10) / 20: exp (dB * this) is the gain of a reduction

    int lookaheadSamplesFor (float ms) const noexcept
    {
        return juce::roundToInt (static_cast<double> (ms) * 0.001 * sampleRate);
    }

    /// The one-pole coefficient of a time constant: 63 % of the way after `ms`.
    float coefficientFor (float ms) const noexcept
    {
        return static_cast<float> (std::exp (-1000.0 / (static_cast<double> (ms) * sampleRate)));
    }

    void applyParameters() noexcept
    {
        threshold = getParameter (thresholdDb);
        halfKnee = 0.5f * getParameter (kneeDb);
        kneeStartPower = std::pow (10.0f, (threshold - halfKnee) * 0.1f);
        slope = 1.0f - 1.0f / getParameter (ratio);
        attackCoefficient = coefficientFor (getParameter (attackMs));
        releaseCoefficient = coefficientFor (getParameter (releaseMs));
        rmsDetection = getParameter (rms) >= 0.5f;
        makeup.setGainDecibels (getParameter (makeupDb));

        // the high-pass comes back into the side chain from rest, not with the state it was switched off with
        const auto wasFiltered = sidechain.active;
        sidechain.active = false;
        if (getParameter (sidechainOn) >= 0.5f)
        {
            sidechain.set (juce::dsp::IIR::ArrayCoefficients<double>::makeHighPass (sampleRate, static_cast<double> (getParameter (sidechainHz))));
            if (! wasFiltered)
                sidechain.clearState();
        }
    }

    double sampleRate = 0.0;

    // render thread, from applyParameters()
    float threshold = 0.0f;         ///< dB
    float halfKnee = 0.0f;          ///< dB
    float kneeStartPower = 1.0f;    ///< the level under which the curve asks for nothing, as a power
    float slope = 0.0f;             ///< dB of reduction per dB over the threshold
    float attackCoefficient = 0.0f, releaseCoefficient = 0.0f, meanSquareCoefficient = 0.0f;
    bool rmsDetection = true;

    // render thread, the detector's state
    float reductionDb = 0.0f;       ///< the smoothed reduction, >= 0
    std::vector<float> meanSquares;   ///< one per channel
    Biquad<double> sidechain;         ///< the side chain's high-pass: in double, its corner is far under the sample rate

    int lookaheadSamples = 0;                                               // from prepare(): the latency the plan compensates
    int maxLookaheadSamples = 0;
    int mask = 0;
    std::vector<std::vector<float>> lines;   ///< the signal waiting for its gain, one line per channel
    int writeIndex = 0;                      // render thread
    juce::dsp::Gain<float> makeup;
    std::atomic<float> gainReductionPeakDb { 0.0f };
};

} // namespace audium
