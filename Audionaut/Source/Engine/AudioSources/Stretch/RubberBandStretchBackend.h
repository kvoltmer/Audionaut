//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include "StretchBackend.h"

#if STRETCH_RUBBERBAND_ENABLED

#include <memory>
#include <JuceHeader.h>
#include <rubberband/RubberBandStretcher.h>

#include "StretchOutputFifo.h"

namespace audium {

/**
 * Rubber Band Library, R3 ("finer") engine in real-time mode (GPL-2.0 or
 * later; a commercial licence exists). Evaluation only unless licensed:
 * this backend must stay out of the Mac App Store build.
 *
 * Push/pull: process() feeds the library and drains whatever it has
 * rendered into a FIFO kept one block ahead. Alignment follows the
 * library's own recipe - pad the start with getPreferredStartPad() zeros,
 * trim getStartDelay() output samples.
 */
class RubberBandStretchBackend : public StretchBackend
{
public:
    StretchEngine engine() const override { return StretchEngine::RubberBand; }

    void prepare (int numChannels, double sampleRate, int maxBlockSize, double maxSpeedRatio) override;
    int maxInputLength (int maxBlockSize, double maxSpeedRatio) const override;
    int primeInputLength (double ratio) const override;
    void prime (const float* const* input, int numInput, double ratio) override;
    int inputForOutput (int numOutput, double ratio) override;
    void process (const float* const* input, int numInput,
                  float* const* output, int numOutput, double ratio) override;

private:
    void applyRatio (double ratio);
    void feed (const float* const* input, int numInput);
    void drain();

    std::unique_ptr<RubberBand::RubberBandStretcher> stretcher;
    StretchOutputFifo fifo;
    juce::AudioBuffer<float> retrieveScratch, zeros;
    int numChannels = 0;
    int headroom = 0;
    int maxProcessSize = 0;
    int pendingDiscard = 0;
    double currentRatio = 0.0;
};

} // namespace audium

#endif // STRETCH_RUBBERBAND_ENABLED
