//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <vector>

#include "Biquad.h"
#include "Engine/Effects/Effect.h"

namespace audium
{

/**
 * EQ: a high-pass, a low shelf, a peaking mid band at a fixed width and a
 * high shelf, each a biquad per channel. The coefficients come from juce_dsp's array makers
 * (no allocation) and are recomputed on the render thread when a parameter
 * changed; a band at 0 dB, or the high-pass while off, is skipped, and
 * starts from a cleared delay line when it comes back.
 */
class Eq3Effect final : public Effect
{
public:
    enum Parameter { lowDb = 0, lowHz, midDb, midHz, highDb, highHz, hpOn, hpHz };

    static constexpr std::array<EffectParameterInfo, 8> parameterInfos {{
        { "low_db",  "Low",            "dB",  -18.0f,    18.0f,    0.0f, false, "Low",       "Gain" },
        { "low_hz",  "Low freq",       "Hz",   40.0f,   600.0f,  120.0f, false, "Low",       "Freq" },
        { "mid_db",  "Mid",            "dB",  -18.0f,    18.0f,    0.0f, false, "Mid",       "Gain" },
        { "mid_hz",  "Mid freq",       "Hz",  200.0f,  6000.0f, 1000.0f, false, "Mid",       "Freq" },
        { "high_db", "High",           "dB",  -18.0f,    18.0f,    0.0f, false, "High",      "Gain" },
        { "high_hz", "High freq",      "Hz", 1500.0f, 16000.0f, 6000.0f, false, "High",      "Freq" },
        { "hp_on",   "High-pass",      "",      0.0f,     1.0f,    0.0f, true,  "High-pass", "On" },
        { "hp_hz",   "High-pass freq", "Hz",   10.0f,  1000.0f,   80.0f, false, "High-pass", "Freq" },
    }};

    static EffectTypeInfo typeInfo() { return { "builtin:eq3", "EQ", "EQ", "A high-pass, low and high shelves and a mid band." }; }

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
            band.clearState();
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
                    samples[i] = band.process (s, samples[i]);
            }
        }
    }

private:
    using Band = Biquad<float>;

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
            bands[2].set (Makers::makePeakFilter (sampleRate, hz (getParameter (midHz)), midQ, gainOf (getParameter (midDb))));

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
    static constexpr float midQ = 1.0f;   ///< about 1.4 octaves wide

    double sampleRate = 0.0;
    std::array<Band, 4> bands;
};

} // namespace audium
