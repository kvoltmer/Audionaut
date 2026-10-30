//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include "Engine/Effects/Effect.h"

namespace audium
{

/**
 * Trim: gain in dB and a polarity flip. The gain ramps over 10 ms, through
 * zero on a flip, so neither clicks.
 */
class TrimEffect final : public Effect
{
public:
    enum Parameter { gainDb = 0, polarity };

    static constexpr std::array<EffectParameterInfo, 2> parameterInfos {{
        { "gain_db",  "Gain",     "dB", -24.0f, 24.0f, 0.0f },
        { "polarity", "Polarity", "",     0.0f,  1.0f, 0.0f, true },
    }};

    static EffectTypeInfo typeInfo() { return { "builtin:trim", "Trim" }; }

    TrimEffect() : Effect (typeInfo(), parameterInfos) {}

    void prepare (double sampleRate, int maxBlockSize, int numChannels) override
    {
        juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32> (maxBlockSize), static_cast<juce::uint32> (numChannels) };
        gain.setRampDurationSeconds (0.01);
        gain.prepare (spec);
        applyParameters();
        gain.reset();   // the first block starts at the set gain, not at a ramp from unity
    }

    void reset() noexcept override
    {
        gain.reset();
    }

    void process (juce::dsp::AudioBlock<float>& block) noexcept override
    {
        if (takeParameterChange())
            applyParameters();

        juce::dsp::ProcessContextReplacing<float> context (block);
        gain.process (context);
    }

private:
    void applyParameters() noexcept
    {
        const auto linear = juce::Decibels::decibelsToGain (getParameter (gainDb));
        gain.setGainLinear (getParameter (polarity) >= 0.5f ? -linear : linear);
    }

    juce::dsp::Gain<float> gain;
};

} // namespace audium
