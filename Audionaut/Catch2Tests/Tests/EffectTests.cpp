//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>

#include "Engine/Effects/EffectRegistry.h"
#include "Engine/Effects/InsertChain.h"
#include "Engine/Effects/MixPlanContainer.h"
#include "Engine/Effects/MixerPlanner.h"
#include "Engine/Effects/BuiltIn/CompressorEffect.h"
#include "Engine/Effects/BuiltIn/DelayEffect.h"
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

        THEN ("Trim, EQ, Compressor and Delay are registered under their type ids")
        {
            REQUIRE (registry.getTypes().size() == 4);
            REQUIRE (registry.hasType ("builtin:trim"));
            REQUIRE (registry.hasType ("builtin:eq3"));
            REQUIRE (registry.hasType ("builtin:compressor"));
            REQUIRE (registry.hasType ("builtin:delay"));
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
                    // below the compressor's threshold; the delay passes half
                    // of it dry, and its first echo (350 ms) lies past these
                    // eight blocks
                    const auto expected = type.id == "builtin:delay" ? 0.01f : 0.02f;
                    auto buffer = makeSine (numChannels, 8, 1000.0, 0.02f);
                    run (*effect, buffer);
                    for (auto c = 0; c < numChannels; ++c)
                        REQUIRE (settledPeak (buffer, c) == Approx (expected).margin (0.002f));
                }
            }
        }
    }
}

SCENARIO ("an EQ band that comes back on starts from rest", "[engine][effects]")
{
    GIVEN ("an EQ whose boosted low shelf has filtered a signal")
    {
        Eq3Effect eq;
        eq.prepare (kSampleRate, kBlockSize, 1);
        eq.setParameter (Eq3Effect::lowDb, 12.0f);
        auto signal = makeSine (1, 4, 100.0, 0.5f);
        run (eq, signal);
        REQUIRE (settledPeak (signal) > 0.5f);

        WHEN ("the band goes to 0 dB for a block and then comes back")
        {
            eq.setParameter (Eq3Effect::lowDb, 0.0f);
            auto passed = makeSine (1, 1, 100.0, 0.5f);
            run (eq, passed);
            eq.setParameter (Eq3Effect::lowDb, 12.0f);

            THEN ("silence in is silence out: nothing from before the band was off rings")
            {
                juce::AudioBuffer<float> silence (1, kBlockSize);
                silence.clear();
                run (eq, silence);
                REQUIRE (silence.getMagnitude (0, 0, kBlockSize) == 0.0f);
            }
        }

        WHEN ("the band's gain changes while it stays on")
        {
            eq.setParameter (Eq3Effect::lowDb, 11.0f);

            THEN ("its delay line is kept: the next block continues the filtered signal, no step to zero")
            {
                juce::AudioBuffer<float> silence (1, kBlockSize);
                silence.clear();
                run (eq, silence);
                REQUIRE (silence.getMagnitude (0, 0, 4) > 0.0f);
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

SCENARIO ("an insert chain keeps what it cannot create", "[engine][effects][inserts]")
{
    GIVEN ("a chain with a planner that knows the built-ins only")
    {
        auto registry = std::make_shared<EffectRegistry>();
        registry->addBuiltInEffects();
        MixerPlanner planner (registry, std::make_shared<MixPlanContainer>());
        InsertChain chain (&planner);

        WHEN ("a chain of a trim, an effect of an unknown type and an EQ is read")
        {
            json input;
            input["inserts"] = json::array ({ { { "type", "builtin:trim" }, { "enabled", true }, { "state", { { "gain_db", -6.0 } } } },
                                              { { "type", "builtin:future" }, { "enabled", false }, { "state", { { "amount", 0.5 } } } },
                                              { { "type", "builtin:eq3" }, { "enabled", true }, { "state", json::object() } } });
            chain.readFromJson (input);

            THEN ("the two known types are slots and the unknown one is kept aside")
            {
                REQUIRE (chain.size() == 2);
                REQUIRE (chain.getSlot (0)->getEffect().getType().id == "builtin:trim");
                REQUIRE (chain.getSlot (1)->getEffect().getType().id == "builtin:eq3");
                REQUIRE (chain.getNumUnresolved() == 1);
            }

            THEN ("writing the chain puts the unknown element back where it was, as it was")
            {
                json output;
                chain.writeToJson (output);
                REQUIRE (output["inserts"].size() == 3);
                REQUIRE (output["inserts"][0]["type"] == "builtin:trim");
                REQUIRE (output["inserts"][1]["type"] == "builtin:future");
                REQUIRE (output["inserts"][1]["enabled"] == false);
                REQUIRE (output["inserts"][1]["state"]["amount"].get<double>() == Catch::Approx (0.5));
                REQUIRE (output["inserts"][2]["type"] == "builtin:eq3");
            }

            THEN ("removing the slots leaves the element, so the chain still writes it")
            {
                REQUIRE (chain.remove (1));
                REQUIRE (chain.remove (0));
                REQUIRE (chain.isEmpty());
                json output;
                chain.writeToJson (output);
                REQUIRE (output["inserts"].size() == 1);
                REQUIRE (output["inserts"][0]["type"] == "builtin:future");

                chain.clear();
                json cleared;
                chain.writeToJson (cleared);
                REQUIRE_FALSE (cleared.contains ("inserts"));
            }
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

    GIVEN ("a delay of 100 ms, feedback 0.5, fully wet")
    {
        DelayEffect delay;
        delay.setParameter (DelayEffect::timeMs, 100.0f);
        delay.setParameter (DelayEffect::feedback, 0.5f);
        delay.setParameter (DelayEffect::mix, 1.0f);
        delay.prepare (kSampleRate, kBlockSize, 2);
        const auto period = static_cast<int> (0.1 * kSampleRate);   // 4800 samples

        THEN ("an impulse comes back after 100 ms, then at half every 100 ms, on both channels and nothing in between")
        {
            juce::AudioBuffer<float> buffer (2, 16 * kBlockSize);   // 4096 samples: the first echo lands in the second run
            buffer.clear();
            buffer.setSample (0, 0, 1.0f);
            buffer.setSample (1, 0, 0.5f);
            juce::AudioBuffer<float> output (2, 4 * period);
            output.clear();
            for (auto start = 0; start < output.getNumSamples(); start += buffer.getNumSamples())
            {
                if (start > 0)
                    buffer.clear();
                run (delay, buffer);
                const auto count = std::min (buffer.getNumSamples(), output.getNumSamples() - start);
                for (auto c = 0; c < 2; ++c)
                    output.copyFrom (c, start, buffer, c, 0, count);
            }

            for (auto c = 0; c < 2; ++c)
            {
                const auto impulse = c == 0 ? 1.0f : 0.5f;
                REQUIRE (output.getSample (c, 0) == Approx (0.0f));   // fully wet: no dry impulse
                REQUIRE (output.getSample (c, period) == Approx (impulse).margin (1.0e-5f));
                REQUIRE (output.getSample (c, 2 * period) == Approx (0.5f * impulse).margin (1.0e-5f));
                REQUIRE (output.getSample (c, 3 * period) == Approx (0.25f * impulse).margin (1.0e-5f));
                REQUIRE (output.getMagnitude (c, 1, period - 1) == Approx (0.0f));
                REQUIRE (output.getMagnitude (c, period + 1, period - 1) == Approx (0.0f));
            }
        }

        THEN ("a fractional delay time passes a tone at its level: the read interpolates between samples")
        {
            delay.setParameter (DelayEffect::feedback, 0.0f);
            delay.setParameter (DelayEffect::timeMs, 10.01f);   // 480.48 samples
            delay.reset();
            auto tone = makeSine (1, 16, 1000.0, 0.5f);
            run (delay, tone);
            REQUIRE (settledPeak (tone) == Approx (0.5f).margin (1.0e-3f));
        }

        THEN ("the tail is the time times the repeats down to -60 dB")
        {
            REQUIRE (delay.getTailSeconds() == Approx (1.0));   // 10 repeats of 0.5 = -60 dB
            delay.setParameter (DelayEffect::feedback, 0.0f);
            REQUIRE (delay.getTailSeconds() == Approx (0.1));
        }

        THEN ("at half mix the dry signal passes at half and reset() empties the line")
        {
            delay.setParameter (DelayEffect::mix, 0.5f);
            delay.reset();
            auto tone = makeSine (2, 8, 1000.0, 0.5f);   // 2048 samples, before the first echo
            run (delay, tone);
            REQUIRE (settledPeak (tone) == Approx (0.25f).margin (1.0e-4f));

            delay.reset();
            juce::AudioBuffer<float> silence (2, 8 * kBlockSize);
            silence.clear();
            run (delay, silence);
            REQUIRE (silence.getMagnitude (0, 0, silence.getNumSamples()) == Approx (0.0f));
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
