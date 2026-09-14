//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include "StretchBackend.h"

#if STRETCH_BUNGEE_ENABLED

#include <memory>
#include <JuceHeader.h>

#include "StretchOutputFifo.h"

namespace audium {

/**
 * Bungee (MPL-2.0): a phase-vocoder stretcher with a grain API. The public
 * headers are Eigen-free; the library sources compile with their own Eigen
 * and PFFFT (see the build's `bungee` flag scheme).
 *
 * The adapter carries its own copy of Bungee::Stream's forward-playback
 * logic (BungeeStream, in the .cpp) because the library's Stream cannot be
 * reset without reallocating. Alignment: the stream's position mapping is
 * not exact at the start, so the adapter measures where the raw pipeline
 * puts input[0] - a step signal through a scratch instance at the
 * requested ratio - and pads or trims accordingly (cached per ratio; the
 * measurement costs a few milliseconds when the ratio changes).
 */
class BungeeStretchBackend : public StretchBackend
{
public:
    BungeeStretchBackend();
    ~BungeeStretchBackend() override;

    StretchEngine engine() const override { return StretchEngine::Bungee; }

    void prepare (int numChannels, double sampleRate, int maxBlockSize, double maxSpeedRatio) override;
    int maxInputLength (int maxBlockSize, double maxSpeedRatio) const override;
    int primeInputLength (double ratio) const override;
    void prime (const float* const* input, int numInput, double ratio) override;
    int inputForOutput (int numOutput, double ratio) override;
    void process (const float* const* input, int numInput,
                  float* const* output, int numOutput, double ratio) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;

    StretchOutputFifo fifo;
    juce::AudioBuffer<float> renderScratch;
    int numChannels = 0;
    int headroom = 0;
    int maxInputPerCall = 0;
    int windowFrames = 0;   // the stretcher's maximum input frame count (its analysis window)
    int pendingDiscard = 0; // output samples still to trim (input[0] not reached yet)

    // Raw pipeline, no alignment correction - what the calibrator runs.
    void primeRaw (const float* const* input, int numInput, double ratio);
    void pushRendered (int rendered);

    /// The input frame the raw pipeline's first output frame maps to at
    /// @p ratio (positive: input skipped; negative: output ahead of the start).
    int measureStartOffset (double ratio);

    std::unique_ptr<BungeeStretchBackend> calibrator;   // absent in the calibrator itself
    bool isCalibratorInstance = false;
    juce::AudioBuffer<float> stepScratch, zeros, calibrationOut;
    double calibratedRatio = 0.0;
    int calibratedOffset = 0;
    int preparedBlockSize = 0;
};

} // namespace audium

#endif // STRETCH_BUNGEE_ENABLED
