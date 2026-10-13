//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <memory>
#include <JuceHeader.h>
#include <rubberband/RubberBandStretcher.h>

#include "StretchOutputFifo.h"

namespace audium {

/**
 * The pitch-preserving time-stretcher behind StretchAudioSource: Rubber
 * Band Library's R3 ("finer") engine in real-time mode (GPL-2.0 or later;
 * a commercial licence exists).
 *
 * The node drives it like this: after a position change it pulls
 * primeInputLength() samples from upstream and hands them to prime(); from
 * then on, for each rendered block it asks inputForOutput() how much input
 * to pull and calls process() with exactly that much.
 *
 * Alignment contract: the first output sample process() produces after
 * prime() corresponds to the FIRST sample handed to prime() - the priming
 * input is look-ahead, not skipped material. Rubber Band gets there with
 * its own recipe: pad the start with getPreferredStartPad() zeros and trim
 * getStartDelay() output samples.
 *
 * Push/pull: process() feeds the library in chunks and drains whatever it
 * has rendered into a FIFO kept one block ahead. Every buffer is sized in
 * prepare() from the library's window geometry at the extreme ratios, so
 * a voice costs what it needs and nothing here allocates afterwards.
 *
 * Real-time contract: prepare() may allocate; everything else runs on the
 * audio thread and must not.
 */
class RubberBandStretchBackend
{
public:
    /// Allocates for @p numChannels at @p sampleRate; @p maxBlockSize is the
    /// largest numOutput a process() call will ask for, the ratio bounds
    /// (input samples per output sample) size the buffers.
    void prepare (int numChannels, double sampleRate, int maxBlockSize,
                  double minSpeedRatio, double maxSpeedRatio);

    /// The largest input count the node may hand to a single prime() or
    /// process() call, for the node's scratch buffer.
    int maxInputLength() const noexcept    { return maxInput; }

    /// Input samples the node must supply to prime() at @p ratio: enough for
    /// the library to fill its first window and leave one block, the
    /// headroom and a hop of slack in the FIFO once the start delay is
    /// trimmed - no more, since the prime runs inside one audio callback.
    int primeInputLength (double ratio) const noexcept;

    /// Restarts on a fresh stream at @p ratio, consuming @p numInput
    /// look-ahead samples so the next process() output starts at input[0].
    void prime (const float* const* input, int numInput, double ratio);

    /** The same prime in pieces, for a standby stretcher warmed over
        several callbacks: beginPrime resets the stretcher and applies the
        ratio; the caller then feeds getStartPad() zeros through feedPadding
        and primeInputLength (ratio) samples through feedInput, in whatever
        chunks it likes. Once both are in, the stretcher is exactly where
        prime() would have left it. */
    void beginPrime (double ratio);
    void feedPadding (int numZeros);
    void feedInput (const float* const* input, int numInput)    { feed (input, numInput); }
    int getStartPad() const noexcept    { return startPad; }

    /// Input samples to supply to the next process() producing @p numOutput
    /// samples at @p ratio.
    int inputForOutput (int numOutput, double ratio);

    /// Consumes @p numInput samples and renders exactly @p numOutput.
    void process (const float* const* input, int numInput,
                  float* const* output, int numOutput, double ratio);

    /// Blocks the FIFO could not fill / samples it had to drop (diagnostics).
    int getUnderruns() const noexcept      { return fifo.getUnderruns(); }
    int getOverflows() const noexcept      { return fifo.getOverflows(); }

private:
    int hopSlack() const noexcept;
    void applyRatio (double ratio);
    void feed (const float* const* input, int numInput);
    void drain();

    std::unique_ptr<RubberBand::RubberBandStretcher> stretcher;
    StretchOutputFifo fifo;
    juce::AudioBuffer<float> retrieveScratch, zeros;
    int numChannels = 0;
    int blockSize = 0;
    int headroom = 0;
    int windowSize = 0;       // input the library holds before it renders anything
    int startPad = 0;         // zeros fed ahead of the material...
    int startDelay = 0;       // ...and the output trimmed for them
    int maxProcessSize = 0;   // one feed chunk
    int maxInput = 0;
    int pendingDiscard = 0;
    double currentRatio = 0.0;
};

} // namespace audium
