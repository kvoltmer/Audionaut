//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Engine/Separation/DemucsBackend.h"
#include "Engine/Separation/DemucsConfig.h"
#include "Engine/Separation/DemucsModelStore.h"

// The real Demucs backend. The inference scenario is hidden ([.]) because it
// needs the 84 MB weights and takes a while: run it with
//
//   AudionautTests "[demucs]"
//
// after installing the model through the app, or with AUDIONAUT_DEMUCS_MODEL
// pointing at the weights file.

using namespace audium;

namespace {

juce::File modelFileForTests()
{
    if (auto path = juce::SystemStats::getEnvironmentVariable ("AUDIONAUT_DEMUCS_MODEL", {}); path.isNotEmpty())
        return juce::File (path);

    return DemucsModelStore::createDefault().getModelFile();
}

/// Five seconds of a stereo mix of two tones, so the stems have something to
/// pull apart and add back up.
juce::AudioBuffer<float> makeTestMix (int sampleRate, double seconds)
{
    const auto numSamples = static_cast<int> (sampleRate * seconds);
    juce::AudioBuffer<float> mix (2, numSamples);

    for (auto i = 0; i < numSamples; ++i)
    {
        const auto t = static_cast<double> (i) / sampleRate;
        const auto low = 0.3f * static_cast<float> (std::sin (2.0 * juce::MathConstants<double>::pi * 110.0 * t));
        const auto high = 0.2f * static_cast<float> (std::sin (2.0 * juce::MathConstants<double>::pi * 880.0 * t));
        mix.setSample (0, i, low + high);
        mix.setSample (1, i, low - high);
    }

    return mix;
}

} // namespace

SCENARIO ("the Demucs backend reports what it needs", "[engine][separation][demucs-backend]")
{
    const auto missing = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("no-such-model.bin");
    DemucsBackend backend (missing, 2);

    REQUIRE (backend.getName() == "demucs");
    REQUIRE (backend.getRequiredSampleRate() == 44100);

    juce::String reason;
    REQUIRE_FALSE (backend.isReady (reason));

    if (DemucsBackend::isCompiledIn())
        REQUIRE (reason.contains ("not installed"));
    else
        REQUIRE (reason.contains ("without Demucs"));

    juce::AudioBuffer<float> input (2, 4410);
    input.clear();
    std::vector<juce::AudioBuffer<float>> stems;
    juce::String error;
    REQUIRE_FALSE (backend.separate (input, stems, nullptr, error));
    REQUIRE (error.isNotEmpty());
    REQUIRE (stems.empty());
}

SCENARIO ("the Demucs backend separates a mix into four stems that sum back to it", "[.][demucs]")
{
    const auto modelFile = modelFileForTests();

    if (! DemucsBackend::isCompiledIn())
        SKIP ("built without Demucs");

    if (! modelFile.existsAsFile())
        SKIP ("no model at " << modelFile.getFullPathName());

    DemucsBackend backend (modelFile, juce::jmax (1, juce::SystemStats::getNumPhysicalCpus()));

    juce::String reason;
    REQUIRE (backend.isReady (reason));

    const auto mix = makeTestMix (backend.getRequiredSampleRate(), 5.0);

    std::vector<juce::AudioBuffer<float>> stems;
    juce::String error;
    std::vector<double> reported;

    const auto ok = backend.separate (mix, stems,
                                      [&reported] (double fraction, const juce::String&)
                                      {
                                          reported.push_back (fraction);
                                          return true;
                                      },
                                      error);

    INFO ("error: " << error);
    REQUIRE (ok);
    REQUIRE (stems.size() == 4);
    REQUIRE_FALSE (reported.empty());
    REQUIRE (reported.back() == Catch::Approx (1.0));

    juce::AudioBuffer<float> sum (2, mix.getNumSamples());
    sum.clear();

    for (const auto& stem : stems)
    {
        REQUIRE (stem.getNumChannels() == 2);
        REQUIRE (stem.getNumSamples() == mix.getNumSamples());

        for (auto channel = 0; channel < 2; ++channel)
        {
            for (auto i = 0; i < stem.getNumSamples(); ++i)
                REQUIRE (std::isfinite (stem.getSample (channel, i)));

            sum.addFrom (channel, 0, stem, channel, 0, stem.getNumSamples());
        }
    }

    // Demucs' stems reconstruct the mix closely; compare energies rather
    // than samples, away from the edges the model pads.
    auto energy = [] (const juce::AudioBuffer<float>& buffer, int from, int to)
    {
        double total = 0.0;
        for (auto channel = 0; channel < buffer.getNumChannels(); ++channel)
            for (auto i = from; i < to; ++i)
                total += buffer.getSample (channel, i) * buffer.getSample (channel, i);
        return total;
    };

    const auto from = mix.getNumSamples() / 4;
    const auto to = mix.getNumSamples() * 3 / 4;
    const auto mixEnergy = energy (mix, from, to);
    const auto sumEnergy = energy (sum, from, to);

    REQUIRE (mixEnergy > 0.0);
    REQUIRE (sumEnergy == Catch::Approx (mixEnergy).epsilon (0.25));
}
