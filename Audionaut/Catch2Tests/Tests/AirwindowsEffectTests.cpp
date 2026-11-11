//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>
#include <cstdlib>

#include "Engine/Effects/EffectRegistry.h"
#include "Engine/Effects/Airwindows/AirwindowsEffect.h"

using namespace audium;
using Catch::Approx;

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int kBlockSize = 256;

    /// Deterministic noise at `amplitude` into every channel (a different sequence per channel).
    juce::AudioBuffer<float> makeNoise (int numChannels, int numSamples, float amplitude = 0.3f, int seed = 1)
    {
        juce::AudioBuffer<float> buffer (numChannels, numSamples);
        for (auto c = 0; c < numChannels; ++c)
        {
            juce::Random random (seed * 31 + c);
            for (auto i = 0; i < numSamples; ++i)
                buffer.setSample (c, i, amplitude * (random.nextFloat() * 2.0f - 1.0f));
        }
        return buffer;
    }

    void run (Effect& effect, juce::AudioBuffer<float>& buffer)
    {
        juce::dsp::AudioBlock<float> whole (buffer);
        for (size_t start = 0; start < whole.getNumSamples(); start += kBlockSize)
        {
            auto block = whole.getSubBlock (start, std::min<size_t> (kBlockSize, whole.getNumSamples() - start));
            effect.process (block);
        }
    }

    bool allFinite (const juce::AudioBuffer<float>& buffer)
    {
        for (auto c = 0; c < buffer.getNumChannels(); ++c)
            for (auto i = 0; i < buffer.getNumSamples(); ++i)
                if (! std::isfinite (buffer.getSample (c, i)))
                    return false;
        return true;
    }

    /// Every parameter half way, so a test runs an effect away from its defaults (a reverb's wetness, a compressor's amount).
    void setMidway (Effect& effect)
    {
        for (auto i = 0; i < effect.getNumParameters(); ++i)
            effect.setParameter (i, 0.5f);
    }
}

SCENARIO ("the Airwindows catalog registers after the built-ins", "[engine][effects][airwindows]")
{
    EffectRegistry registry;
    registry.addBuiltInEffects();
    registry.addAirwindowsEffects();

    const auto descriptors = airwindows::getDescriptors();
    REQUIRE (descriptors.size() == 23);
    REQUIRE (registry.getTypes().size() == 4 + descriptors.size());
    REQUIRE (registry.getTypes()[0].id == "builtin:trim");

    for (const auto& descriptor : descriptors)
    {
        const auto id = airwindows::typeIdFor (descriptor);
        INFO (id);
        REQUIRE (registry.hasType (id));
        REQUIRE (juce::String (descriptor.category).isNotEmpty());
        REQUIRE (juce::String (descriptor.description).isNotEmpty());

        auto effect = registry.create (id);
        REQUIRE (effect != nullptr);
        REQUIRE (effect->getType().id == id);
        REQUIRE (effect->getType().category == descriptor.category);
        REQUIRE (effect->getNumParameters() <= Effect::maxParameters);

        // every parameter id is unique and every default lies in the range
        for (auto i = 0; i < effect->getNumParameters(); ++i)
        {
            const auto& info = effect->getParameters()[static_cast<size_t> (i)];
            REQUIRE (effect->getParameterIndex (info.id) == i);
            REQUIRE (info.def >= info.min);
            REQUIRE (info.def <= info.max);
        }
    }

    REQUIRE (registry.hasType ("airwindows:buttercomp2"));
    REQUIRE (registry.hasType ("airwindows:kplated"));
    REQUIRE (registry.create ("airwindows:nothing") == nullptr);
}

SCENARIO ("every Airwindows effect processes one, two and three channels to finite output", "[engine][effects][airwindows]")
{
    for (const auto& descriptor : airwindows::getDescriptors())
    {
        for (auto numChannels = 1; numChannels <= 3; ++numChannels)
        {
            INFO (descriptor.name << ", " << numChannels << " channel(s)");
            airwindows::AirwindowsEffect effect (descriptor);
            effect.prepare (kSampleRate, kBlockSize, numChannels);
            setMidway (effect);

            auto buffer = makeNoise (numChannels, 16 * kBlockSize);
            const auto input = buffer;
            run (effect, buffer);

            REQUIRE (allFinite (buffer));
            REQUIRE (buffer.getMagnitude (0, buffer.getNumSamples()) < 20.0f);   // nothing blew up

            // a block shorter than the prepared size is fine too
            auto shortBuffer = makeNoise (numChannels, kBlockSize / 3);
            run (effect, shortBuffer);
            REQUIRE (allFinite (shortBuffer));
        }
    }
}

SCENARIO ("an Airwindows effect resets to exactly where a fresh one starts", "[engine][effects][airwindows]")
{
    for (const auto& descriptor : airwindows::getDescriptors())
    {
        INFO (descriptor.name);
        airwindows::AirwindowsEffect effect (descriptor);
        effect.prepare (kSampleRate, kBlockSize, 2);
        setMidway (effect);

        // the first pass, from the prepared state
        auto first = makeNoise (2, 24 * kBlockSize, 0.3f, 7);
        run (effect, first);

        // other material in between, then a reset and the same input again
        auto other = makeNoise (2, 40 * kBlockSize, 0.8f, 99);
        run (effect, other);
        effect.reset();

        auto second = makeNoise (2, 24 * kBlockSize, 0.3f, 7);
        run (effect, second);

        // bit for bit: the reset copies the untouched instance over the used one, dither seeds included
        for (auto c = 0; c < 2; ++c)
            for (auto i = 0; i < first.getNumSamples(); ++i)
                REQUIRE (second.getSample (c, i) == first.getSample (c, i));
    }
}

SCENARIO ("a mono channel runs as both sides of the stereo effect, folded back", "[engine][effects][airwindows]")
{
    for (const auto& descriptor : airwindows::getDescriptors())
    {
        INFO (descriptor.name);
        // the same dither seeds for both (Airwindows draws them from rand() when an instance is built)
        airwindows::AirwindowsEffect mono (descriptor), stereo (descriptor);
        std::srand (42);
        mono.prepare (kSampleRate, kBlockSize, 1);
        std::srand (42);
        stereo.prepare (kSampleRate, kBlockSize, 2);
        setMidway (mono);
        setMidway (stereo);

        auto monoBuffer = makeNoise (1, 16 * kBlockSize, 0.3f, 3);
        juce::AudioBuffer<float> stereoBuffer (2, monoBuffer.getNumSamples());
        stereoBuffer.copyFrom (0, 0, monoBuffer, 0, 0, monoBuffer.getNumSamples());
        stereoBuffer.copyFrom (1, 0, monoBuffer, 0, 0, monoBuffer.getNumSamples());

        run (mono, monoBuffer);
        run (stereo, stereoBuffer);

        // the average of the stereo instance's two sides
        for (auto i = 0; i < monoBuffer.getNumSamples(); ++i)
        {
            const auto folded = 0.5f * (stereoBuffer.getSample (0, i) + stereoBuffer.getSample (1, i));
            REQUIRE (monoBuffer.getSample (0, i) == Approx (folded).margin (1.0e-6));
        }
    }
}

SCENARIO ("an Airwindows effect shows its values as Airwindows does and reads typed ones back", "[engine][effects][airwindows]")
{
    EffectRegistry registry;
    registry.addAirwindowsEffects();

    GIVEN ("Logical4, whose threshold reads in dB and whose ratio is squared")
    {
        auto effect = registry.create ("airwindows:logical4");
        const auto threshold = effect->getParameterIndex ("threshold");
        const auto ratio = effect->getParameterIndex ("ratio");
        REQUIRE (threshold >= 0);
        REQUIRE (ratio >= 0);

        REQUIRE (effect->getParameterText (threshold, 0.5f) == "0.00 dB");
        REQUIRE (effect->getParameterText (threshold, 0.0f) == "-20.00 dB");
        REQUIRE (effect->getParameterText (ratio, 1.0f) == "16.00 /1");

        auto value = 0.0f;
        REQUIRE (effect->parseParameterText (threshold, "-10 dB", value));
        REQUIRE (value == Approx (0.25f));
        REQUIRE (effect->parseParameterText (ratio, "4", value));
        REQUIRE (value == Approx (std::sqrt (3.0f / 15.0f)));
    }

    GIVEN ("ADClip8, whose mode reads as a name")
    {
        auto effect = registry.create ("airwindows:adclip8");
        const auto mode = effect->getParameterIndex ("mode");
        REQUIRE (mode >= 0);
        REQUIRE (effect->getParameterText (mode, 0.0f) == "Normal");
        REQUIRE (effect->getParameterText (mode, 1.0f) == "Apothes");
    }

    THEN ("every value a display shows as a number reads back as the same value")
    {
        for (const auto& descriptor : airwindows::getDescriptors())
        {
            airwindows::AirwindowsEffect effect (descriptor);
            for (auto i = 0; i < effect.getNumParameters(); ++i)
            {
                for (const auto value : { 0.1f, 0.5f, 0.9f })
                {
                    const auto text = effect.getParameterText (i, value);
                    INFO (descriptor.name << " " << effect.getParameters()[static_cast<size_t> (i)].id << " = " << value << " shows " << text);
                    REQUIRE (text.isNotEmpty());

                    auto parsed = 0.0f;
                    if (effect.parseParameterText (i, text, parsed))
                        REQUIRE (parsed == Approx (value).margin (0.01f));
                }
            }
        }
    }
}

SCENARIO ("the reverbs and delays ring on after their input stops", "[engine][effects][airwindows]")
{
    for (const auto& descriptor : airwindows::getDescriptors())
    {
        if (descriptor.tailSeconds <= 0.0)
            continue;

        INFO (descriptor.name);
        airwindows::AirwindowsEffect effect (descriptor);
        effect.prepare (kSampleRate, kBlockSize, 2);
        REQUIRE (effect.getTailSeconds() == descriptor.tailSeconds);

        // a short burst at the defaults, then up to three seconds of silence
        const auto burst = static_cast<int> (0.05 * kSampleRate);
        const auto silence = static_cast<int> (std::min (3.0, descriptor.tailSeconds) * kSampleRate);
        juce::AudioBuffer<float> buffer (2, burst + silence);
        buffer.clear();
        auto noise = makeNoise (2, burst, 0.5f, 11);
        for (auto c = 0; c < 2; ++c)
            buffer.copyFrom (c, 0, noise, c, 0, burst);

        run (effect, buffer);

        // something comes out once the input is silent
        const auto after = burst + kBlockSize;
        const auto ringing = std::max (buffer.getRMSLevel (0, after, buffer.getNumSamples() - after),
                                       buffer.getRMSLevel (1, after, buffer.getNumSamples() - after));
        REQUIRE (ringing > 1.0e-5f);
    }
}
