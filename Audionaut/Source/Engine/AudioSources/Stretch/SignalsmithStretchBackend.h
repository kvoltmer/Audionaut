//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include <signalsmith-stretch/signalsmith-stretch.h>

#include "StretchBackend.h"

namespace audium {

/**
 * Signalsmith Stretch (MIT): the shipping engine. Synchronous
 * process(in, out) with the rate implied by the two counts; outputSeek()
 * primes so the first output is aligned with the priming input's start.
 */
class SignalsmithStretchBackend : public StretchBackend
{
public:
    StretchEngine engine() const override { return StretchEngine::Signalsmith; }

    void prepare (int numChannels, double sampleRate, int maxBlockSize, double maxSpeedRatio) override;
    int maxInputLength (int maxBlockSize, double maxSpeedRatio) const override;
    int primeInputLength (double ratio) const override;
    void prime (const float* const* input, int numInput, double ratio) override;
    int inputForOutput (int numOutput, double ratio) override;
    void process (const float* const* input, int numInput,
                  float* const* output, int numOutput, double ratio) override;

private:
    signalsmith::stretch::SignalsmithStretch<float> stretch;
    juce::AudioBuffer<float> warmup;
    double inputRemainder = 0.0;
};

} // namespace audium
