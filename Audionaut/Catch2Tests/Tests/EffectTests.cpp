//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>

#include "Engine/Effects/EffectRegistry.h"
#include "Engine/Effects/BuiltIn/CompressorEffect.h"
#include "Engine/Effects/BuiltIn/Eq3Effect.h"
#include "Engine/Effects/BuiltIn/TrimEffect.h"

using namespace audium;
using Catch::Approx;

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 256;

    /// A sine of `hz` at `amplitude` into every channel, `numBlocks` blocks long.
    juce::AudioBuffer<float> makeSine (int numChannels, int numBlocks, double hz, float amplitude = 0.5f)
    {
        juce::AudioBuffer<float> buffer (numChannels, numBlocks * kBlockSize);
        for (auto c = 0; c < numChannels; ++c)
            for (auto i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample (c, i, amplitude * static_cast<float> (std::sin (2.0 * juce::MathConstants<double>::pi * hz * i / kSampleRate)));
        return buffer;
    }

    /// Runs the effect over the buffer in blocks of kBlockSize, in place.
    void run (Effect& effect, juce::AudioBuffer<float>& buffer)
    {
        juce::dsp::AudioBlock<float> whole (buffer);
        for (size_t start = 0; start < whole.getNumSamples(); start += kBlockSize)
        {
            auto block = whole.getSubBlock (start, std::min<size_t> (kBlockSize, whole.getNumSamples() - start));
            effect.process (block);
        }
    }

    /// Peak of the last block, where the ramps and envelopes have settled.
    float settledPeak (const juce::AudioBuffer<float>& buffer, int channel = 0)
    {
        return buffer.getMagnitude (channel, buffer.getNumSamples() - kBlockSize, kBlockSize);
    }
}

SCENARIO ("the effect registry creates every built-in for one and two channels", "[engine][effects]")
{
    GIVEN ("a registry with the built-ins")
    {
        EffectRegistry registry;
        registry.addBuiltInEffects();

        THEN ("Trim, EQ and Compressor are registered under their type ids")
        {
            REQUIRE (registry.getTypes().size() == 3);
            REQUIRE (registry.hasType ("builtin:trim"));
            REQUIRE (registry.hasType ("builtin:eq3"));
            REQUIRE (registry.hasType ("builtin:compressor"));
            REQUIRE_FALSE (registry.hasType ("builtin:nothing"));
            REQUIRE (registry.create ("builtin:nothing") == nullptr);
        }

        THEN ("every type prepares for one and two channels and passes a block through")
        {
            for (const auto& type : registry.getTypes())
            {
                for (auto numChannels = 1; numChannels <= 2; ++numChannels)
                {
                    auto effect = registry.create (type.id);
                    REQUIRE (effect != nullptr);
                    REQUIRE (effect->getType().id == type.id);
                    effect->prepare (kSampleRate, kBlockSize, numChannels);

                    // at the defaults every built-in is transparent to a signal
                    // below the compressor's threshold
                    auto buffer = makeSine (numChannels, 8, 1000.0, 0.02f);
                    run (*effect, buffer);
                    for (auto c = 0; c < numChannels; ++c)
                        REQUIRE (settledPeak (buffer, c) == Approx (0.02f).margin (0.002f));
                }
            }
        }
    }
}

SCENARIO ("effect parameters and state round trip deterministically", "[engine][effects]")
{
    GIVEN ("an EQ with a few parameters changed")
    {
        Eq3Effect eq;
        eq.setParameter (Eq3Effect::hpOn, 1.0f);
        eq.setParameter (Eq3Effect::hpHz, 123.456789f);
        eq.setParameter (Eq3Effect::midDb, -3.0f);

        THEN ("parameters are clamped to their range and booleans to 0 or 1")
        {
            eq.setParameter (Eq3Effect::lowDb, 100.0f);
            REQUIRE (eq.getParameter (Eq3Effect::lowDb) == Approx (18.0f));
            eq.setParameter (Eq3Effect::hpOn, 0.3f);
            REQUIRE (eq.getParameter (Eq3Effect::hpOn) == Approx (0.0f));
            REQUIRE (eq.getParameterIndex ("mid_q") == Eq3Effect::midQ);
            REQUIRE (eq.getParameterIndex ("nope") == -1);
            REQUIRE (eq.getParameter (99) == Approx (0.0f));
        }

        THEN ("the state names every parameter by id and is the same twice over")
        {
            const auto state = eq.getState();
            REQUIRE (state.size() == Eq3Effect::parameterInfos.size());
            REQUIRE (state["hp_on"].get<float>() == Approx (1.0f));
            REQUIRE (state["mid_db"].get<float>() == Approx (-3.0f));
            REQUIRE (state.dump() == eq.getState().dump());

            // read into a fresh instance: identical state, including the rounded value
            Eq3Effect other;
            other.setState (state);
            REQUIRE (other.getState().dump() == state.dump());
            REQUIRE (other.getParameter (Eq3Effect::hpHz) == Approx (123.456789f).margin (1.0e-5f));
        }

        THEN ("unknown keys and a non-object are ignored, missing keys keep their value")
        {
            eq.setState (json { { "nope", 1.0 }, { "mid_db", 6.0 } });
            REQUIRE (eq.getParameter (Eq3Effect::midDb) == Approx (6.0f));
            REQUIRE (eq.getParameter (Eq3Effect::hpHz) == Approx (123.456789f).margin (1.0e-5f));
            eq.setState (json::array());
            REQUIRE (eq.getParameter (Eq3Effect::midDb) == Approx (6.0f));
        }
    }
}

SCENARIO ("the built-ins do what their parameters say", "[engine][effects]")
{
    GIVEN ("a -6 dB trim")
    {
        TrimEffect trim;
        trim.setParameter (TrimEffect::gainDb, -6.0f);
        trim.prepare (kSampleRate, kBlockSize, 2);

        THEN ("the signal settles at half its amplitude on both channels")
        {
            auto buffer = makeSine (2, 4, 1000.0);
            run (trim, buffer);
            REQUIRE (settledPeak (buffer, 0) == Approx (0.5f * juce::Decibels::decibelsToGain (-6.0f)).margin (0.002f));
            REQUIRE (settledPeak (buffer, 1) == Approx (0.5f * juce::Decibels::decibelsToGain (-6.0f)).margin (0.002f));
        }

        THEN ("a polarity flip negates the signal")
        {
            trim.setParameter (TrimEffect::gainDb, 0.0f);
            trim.setParameter (TrimEffect::polarity, 1.0f);
            trim.prepare (kSampleRate, kBlockSize, 1);
            auto buffer = makeSine (1, 4, 1000.0);
            const auto reference = makeSine (1, 4, 1000.0);
            run (trim, buffer);
            const auto last = buffer.getNumSamples() - 1;
            REQUIRE (buffer.getSample (0, last) == Approx (-reference.getSample (0, last)).margin (1.0e-5f));
        }
    }

    GIVEN ("an EQ with the high-pass at 500 Hz")
    {
        Eq3Effect eq;
        eq.setParameter (Eq3Effect::hpOn, 1.0f);
        eq.setParameter (Eq3Effect::hpHz, 500.0f);
        eq.prepare (kSampleRate, kBlockSize, 1);

        THEN ("50 Hz is attenuated by more than 30 dB and 5 kHz passes")
        {
            auto low = makeSine (1, 16, 50.0);
            run (eq, low);
            REQUIRE (settledPeak (low) < 0.5f * juce::Decibels::decibelsToGain (-30.0f));

            eq.reset();
            auto high = makeSine (1, 16, 5000.0);
            run (eq, high);
            REQUIRE (settledPeak (high) == Approx (0.5f).margin (0.01f));
        }

        THEN ("a +6 dB mid band at 1 kHz lifts 1 kHz by 6 dB")
        {
            eq.setParameter (Eq3Effect::hpOn, 0.0f);
            eq.setParameter (Eq3Effect::midDb, 6.0f);
            eq.setParameter (Eq3Effect::midHz, 1000.0f);
            eq.reset();
            auto tone = makeSine (1, 16, 1000.0);
            run (eq, tone);
            REQUIRE (settledPeak (tone) == Approx (0.5f * juce::Decibels::decibelsToGain (6.0f)).margin (0.02f));
        }
    }

    GIVEN ("a compressor at -20 dB threshold, ratio 4")
    {
        CompressorEffect compressor;
        compressor.setParameter (CompressorEffect::thresholdDb, -20.0f);
        compressor.setParameter (CompressorEffect::ratio, 4.0f);
        compressor.setParameter (CompressorEffect::attackMs, 1.0f);
        compressor.setParameter (CompressorEffect::releaseMs, 50.0f);
        compressor.prepare (kSampleRate, kBlockSize, 1);

        THEN ("a 0 dBFS tone comes out around -15 dB, a -30 dB tone untouched")
        {
            auto loud = makeSine (1, 32, 1000.0, 1.0f);
            run (compressor, loud);
            // 20 dB over the threshold at 4:1 leaves 5 dB over it: -15 dBFS
            REQUIRE (juce::Decibels::gainToDecibels (settledPeak (loud)) == Approx (-15.0f).margin (1.5f));

            compressor.reset();
            auto quiet = makeSine (1, 8, 1000.0, juce::Decibels::decibelsToGain (-30.0f));
            run (compressor, quiet);
            REQUIRE (juce::Decibels::gainToDecibels (settledPeak (quiet)) == Approx (-30.0f).margin (0.2f));
        }

        THEN ("make-up gain lifts the result")
        {
            compressor.setParameter (CompressorEffect::makeupDb, 6.0f);
            compressor.reset();
            auto quiet = makeSine (1, 8, 1000.0, juce::Decibels::decibelsToGain (-30.0f));
            run (compressor, quiet);
            REQUIRE (juce::Decibels::gainToDecibels (settledPeak (quiet)) == Approx (-24.0f).margin (0.2f));
        }
    }
}
