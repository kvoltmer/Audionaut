//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include "Engine/Separation/SeparationBackend.h"

namespace audium {

/**
 * A stand-in separator for tests and the CLI's --backend fake.
 *
 * Copies the input into the Vocals stem and leaves the others silent, so a
 * caller can tell the stems apart and check that the audio made the round
 * trip unchanged. Records what it was given, and can be told to fail or to
 * cancel partway through.
 */
class FakeSeparationBackend : public SeparationBackend
{
public:
    juce::String getName() const override { return "fake"; }

    int getRequiredSampleRate() const override { return 44100; }

    bool isReady (juce::String& reason) const override
    {
        if (! ready)
            reason = "fake backend not ready";

        return ready;
    }

    bool separate (const juce::AudioBuffer<float>& stereoInput,
                   std::vector<juce::AudioBuffer<float>>& stemsOut,
                   const SeparationProgress& progress,
                   juce::String& error) override
    {
        ++calls;
        lastInput.makeCopyOf (stereoInput);

        // Report a few steps so a cancelling caller has something to
        // cancel on.
        for (auto step = 0; step <= 4; ++step)
        {
            if (progress != nullptr && ! progress (step / 4.0, "Faking stems"))
                return false;
        }

        if (failWith.isNotEmpty())
        {
            error = failWith;
            return false;
        }

        stemsOut.clear();

        for (auto index = 0; index < numStems; ++index)
        {
            juce::AudioBuffer<float> stem (stereoInput.getNumChannels(), stereoInput.getNumSamples());

            if (stemFromIndex (index) == Stem::Vocals)
                stem.makeCopyOf (stereoInput);
            else
                stem.clear();

            stemsOut.push_back (std::move (stem));
        }

        return true;
    }

    /// Set to make separate() fail with this message.
    juce::String failWith;

    /// Set to false to report the backend as unavailable.
    bool ready = true;

    /// What the last separate() call received.
    juce::AudioBuffer<float> lastInput;

    int calls = 0;
};

} // namespace audium
