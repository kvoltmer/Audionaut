//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include "StretchBackend.h"

#if STRETCH_SOUNDTOUCH_ENABLED

#include <JuceHeader.h>
#include <SoundTouch.h>

#include "StretchOutputFifo.h"

namespace audium {

/**
 * SoundTouch (LGPL-2.1): time-domain WSOLA - the cheap baseline. Works on
 * interleaved frames, so the adapter interleaves on the way in and out.
 * It has no position API and its start offset moves with the tempo, so
 * the adapter measures it: a step signal through a scratch instance at
 * the requested tempo tells which input frame the first output frame
 * maps to; priming then pads (offset ahead) or trims (offset behind).
 */
class SoundTouchStretchBackend : public StretchBackend
{
public:
    StretchEngine engine() const override { return StretchEngine::SoundTouch; }

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
    void feedSilence (int numInput);
    void drain();

    /// The input frame the first output frame maps to at @p ratio (positive:
    /// the stretcher skips that much input; negative: it emits that much
    /// before the input starts). Measured, cached per ratio.
    int measureStartOffset (double ratio);

    soundtouch::SoundTouch soundTouch, calibrator;
    StretchOutputFifo fifo;
    std::vector<float> interleavedIn, interleavedOut;
    juce::AudioBuffer<float> deinterleaved;
    int numChannels = 0;
    int headroom = 0;
    int maxInput = 0;
    int pendingDiscard = 0;
    double currentRatio = 0.0;
    double calibratedRatio = 0.0;
    int calibratedOffset = 0;
};

} // namespace audium

#endif // STRETCH_SOUNDTOUCH_ENABLED
