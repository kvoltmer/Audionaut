//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include "Engine/Effects/Effect.h"

namespace audium
{

/**
 * Compressor: juce_dsp's feed-forward compressor (threshold, ratio, attack,
 * release) with a make-up gain after it.
 */
class CompressorEffect final : public Effect
{
public:
    enum Parameter { thresholdDb = 0, ratio, attackMs, releaseMs, makeupDb };

    static constexpr std::array<EffectParameterInfo, 5> parameterInfos {{
        { "threshold_db", "Threshold", "dB", -60.0f,    0.0f,  -18.0f },
        { "ratio",        "Ratio",     ":1",   1.0f,   20.0f,    4.0f },
        { "attack_ms",    "Attack",    "ms",   0.1f,  200.0f,   10.0f },
        { "release_ms",   "Release",   "ms",   5.0f, 2000.0f,  100.0f },
        { "makeup_db",    "Make-up",   "dB",   0.0f,   24.0f,    0.0f },
    }};

    static EffectTypeInfo typeInfo() { return { "builtin:compressor", "Compressor" }; }

    CompressorEffect() : Effect (typeInfo(), parameterInfos) {}

    void prepare (double sampleRate, int maxBlockSize, int numChannels) override
    {
        juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32> (maxBlockSize), static_cast<juce::uint32> (numChannels) };
        compressor.prepare (spec);
        makeup.setRampDurationSeconds (0.01);
        makeup.prepare (spec);
        applyParameters();
        makeup.reset();
    }

    void reset() noexcept override
    {
        compressor.reset();
        makeup.reset();
    }

    void process (juce::dsp::AudioBlock<float>& block) noexcept override
    {
        if (takeParameterChange())
            applyParameters();

        juce::dsp::ProcessContextReplacing<float> context (block);
        compressor.process (context);
        makeup.process (context);
    }

private:
    void applyParameters() noexcept
    {
        compressor.setThreshold (getParameter (thresholdDb));
        compressor.setRatio (getParameter (ratio));
        compressor.setAttack (getParameter (attackMs));
        compressor.setRelease (getParameter (releaseMs));
        makeup.setGainDecibels (getParameter (makeupDb));
    }

    juce::dsp::Compressor<float> compressor;
    juce::dsp::Gain<float> makeup;
};

} // namespace audium
