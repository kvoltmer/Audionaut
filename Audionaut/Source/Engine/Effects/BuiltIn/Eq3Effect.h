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
 * EQ: a high-pass, a low shelf, a peaking mid band and a high shelf, each a
 * biquad per channel. The coefficients come from juce_dsp's array makers
 * (no allocation) and are recomputed on the render thread when a parameter
 * changed; a band at 0 dB, or the high-pass while off, is skipped, and
 * starts from a cleared delay line when it comes back.
 */
class Eq3Effect final : public Effect
{
public:
    enum Parameter { hpOn = 0, hpHz, lowDb, lowHz, midDb, midHz, midQ, highDb, highHz };

    static constexpr std::array<EffectParameterInfo, 9> parameterInfos {{
        { "hp_on",   "High-pass",      "",      0.0f,     1.0f,    0.0f, true },
        { "hp_hz",   "High-pass freq", "Hz",   10.0f,  1000.0f,   80.0f },
        { "low_db",  "Low",            "dB",  -18.0f,    18.0f,    0.0f },
        { "low_hz",  "Low freq",       "Hz",   40.0f,   600.0f,  120.0f },
        { "mid_db",  "Mid",            "dB",  -18.0f,    18.0f,    0.0f },
        { "mid_hz",  "Mid freq",       "Hz",  200.0f,  6000.0f, 1000.0f },
        { "mid_q",   "Mid Q",          "",      0.3f,     6.0f,    1.0f },
        { "high_db", "High",           "dB",  -18.0f,    18.0f,    0.0f },
        { "high_hz", "High freq",      "Hz", 1500.0f, 16000.0f, 6000.0f },
    }};

    static EffectTypeInfo typeInfo() { return { "builtin:eq3", "EQ" }; }

    Eq3Effect() : Effect (typeInfo(), parameterInfos) {}

    void prepare (double sampleRate_, int, int numChannels) override
    {
        sampleRate = sampleRate_;
        for (auto& band : bands)
            band.states.assign (static_cast<size_t> (numChannels), {});
        computeCoefficients();
        reset();
    }

    void reset() noexcept override
    {
        for (auto& band : bands)
            for (auto& state : band.states)
                state = {};
    }

    void process (juce::dsp::AudioBlock<float>& block) noexcept override
    {
        if (takeParameterChange())
            computeCoefficients();

        const auto numChannels = std::min (block.getNumChannels(), bands[0].states.size());
        const auto numSamples = block.getNumSamples();

        for (auto& band : bands)
        {
            if (! band.active)
                continue;

            for (size_t c = 0; c < numChannels; ++c)
            {
                auto* samples = block.getChannelPointer (c);
                auto& s = band.states[c];
                for (size_t i = 0; i < numSamples; ++i)
                {
                    // transposed direct form II
                    const auto x = samples[i];
                    const auto y = band.b0 * x + s.z1;
                    s.z1 = band.b1 * x - band.a1 * y + s.z2;
                    s.z2 = band.b2 * x - band.a2 * y;
                    samples[i] = y;
                }
            }
        }
    }

private:
    struct State { float z1 = 0.0f, z2 = 0.0f; };

    struct Band
    {
        bool active = false;
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
        std::vector<State> states;   // one per channel, sized by prepare()

        void set (const std::array<float, 6>& unnormalised) noexcept
        {
            const auto a0 = unnormalised[3];
            b0 = unnormalised[0] / a0;
            b1 = unnormalised[1] / a0;
            b2 = unnormalised[2] / a0;
            a1 = unnormalised[4] / a0;
            a2 = unnormalised[5] / a0;
            active = true;
        }

        void clearState() noexcept
        {
            for (auto& state : states)
                state = {};
        }
    };

    void computeCoefficients() noexcept
    {
        using Makers = juce::dsp::IIR::ArrayCoefficients<float>;
        if (sampleRate <= 0.0)
            return;

        // frequencies stay below Nyquist whatever the rate
        const auto maxHz = static_cast<float> (sampleRate * 0.45);
        const auto hz = [maxHz] (float value) { return juce::jmin (value, maxHz); };
        const auto gainOf = [] (float db) { return juce::Decibels::decibelsToGain (db); };

        std::array<bool, 4> wasActive;
        for (size_t i = 0; i < bands.size(); ++i)
            wasActive[i] = bands[i].active;

        bands[0].active = false;
        if (getParameter (hpOn) >= 0.5f)
            bands[0].set (Makers::makeHighPass (sampleRate, hz (getParameter (hpHz))));

        bands[1].active = false;
        if (getParameter (lowDb) != 0.0f)
            bands[1].set (Makers::makeLowShelf (sampleRate, hz (getParameter (lowHz)), shelfQ, gainOf (getParameter (lowDb))));

        bands[2].active = false;
        if (getParameter (midDb) != 0.0f)
            bands[2].set (Makers::makePeakFilter (sampleRate, hz (getParameter (midHz)), getParameter (midQ), gainOf (getParameter (midDb))));

        bands[3].active = false;
        if (getParameter (highDb) != 0.0f)
            bands[3].set (Makers::makeHighShelf (sampleRate, hz (getParameter (highHz)), shelfQ, gainOf (getParameter (highDb))));

        // a skipped band kept the state of the last block it filtered,
        // seconds or minutes ago: coming back, it starts from rest instead
        // of ringing with it
        for (size_t i = 0; i < bands.size(); ++i)
            if (bands[i].active && ! wasActive[i])
                bands[i].clearState();
    }

    static constexpr float shelfQ = 0.7071f;

    double sampleRate = 0.0;
    std::array<Band, 4> bands;
};

} // namespace audium
