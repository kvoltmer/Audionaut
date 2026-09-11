//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

// Which pitch-preserving stretch engines this build carries. The build
// system sets these (jucer defines / CMake options); an unset flag means
// the engine is out.
#ifndef STRETCH_BUNGEE_ENABLED
 #define STRETCH_BUNGEE_ENABLED 0
#endif
#ifndef STRETCH_RUBBERBAND_ENABLED
 #define STRETCH_RUBBERBAND_ENABLED 0
#endif
#ifndef STRETCH_SOUNDTOUCH_ENABLED
 #define STRETCH_SOUNDTOUCH_ENABLED 0
#endif

namespace audium {

/**
 * The pitch-preserving time-stretch engines StretchAudioSource can run on.
 * Signalsmith is the one that ships; the others are here to be evaluated
 * against it (see StretchEngines and the stretch-eval CLI verb).
 */
enum class StretchEngine
{
    Signalsmith = 0,   ///< Signalsmith Stretch (MIT) - header-only, the default
    Bungee,            ///< Bungee (MPL-2.0) - phase vocoder, Eigen + PFFFT
    RubberBand,        ///< Rubber Band R3 (GPL / commercial) - evaluation only unless licensed
    SoundTouch         ///< SoundTouch (LGPL-2.1) - time-domain WSOLA baseline
};

/**
 * One time-stretch engine behind StretchAudioSource.
 *
 * The node drives every backend the same way: after a position change it
 * pulls primeInputLength() samples from upstream and hands them to prime();
 * from then on, for each rendered block it asks inputForOutput() how much
 * input to pull and calls process() with exactly that much.
 *
 * Alignment contract: the first output sample process() produces after
 * prime() corresponds to the FIRST sample handed to prime() - the priming
 * input is look-ahead, not skipped material. The stretchers differ in how
 * they get there (seek-and-discard, pad-and-trim), and each adapter hides
 * that.
 *
 * Real-time contract: prepare() may allocate; everything else runs on the
 * audio thread and must not.
 */
class StretchBackend
{
public:
    virtual ~StretchBackend() = default;

    virtual StretchEngine engine() const = 0;

    /// Allocates for @p numChannels at @p sampleRate; @p maxBlockSize is the
    /// largest numOutput a process() call will ask for, @p maxSpeedRatio the
    /// largest ratio (both bound the internal buffers).
    virtual void prepare (int numChannels, double sampleRate, int maxBlockSize, double maxSpeedRatio) = 0;

    /// The largest input count the node may hand to a single prime() or
    /// process() call, for the node's scratch buffer.
    virtual int maxInputLength (int maxBlockSize, double maxSpeedRatio) const = 0;

    /// Input samples the node must supply to prime() at @p ratio.
    virtual int primeInputLength (double ratio) const = 0;

    /// Restarts on a fresh stream at @p ratio, consuming @p numInput
    /// look-ahead samples so the next process() output starts at input[0].
    virtual void prime (const float* const* input, int numInput, double ratio) = 0;

    /// Input samples to supply to the next process() producing @p numOutput
    /// samples at @p ratio (input samples per output sample).
    virtual int inputForOutput (int numOutput, double ratio) = 0;

    /// Consumes @p numInput samples and renders exactly @p numOutput.
    virtual void process (const float* const* input, int numInput,
                          float* const* output, int numOutput, double ratio) = 0;
};

/**
 * The engine registry: which engines this build carries, their names (the
 * CLI / preference spelling), the process-wide selection, and the factory.
 */
namespace StretchEngines
{
    const char* name (StretchEngine engine);          ///< e.g. "signalsmith"
    const char* displayName (StretchEngine engine);   ///< e.g. "Signalsmith Stretch"
    const char* licence (StretchEngine engine);       ///< e.g. "MIT"
    std::optional<StretchEngine> fromName (const std::string& name);

    /// The engines compiled into this build, Signalsmith first.
    std::vector<StretchEngine> available();
    bool isAvailable (StretchEngine engine);

    /// The engine new voices are prepared with (see StretchAudioSource::
    /// prepareToPlay). Process-wide; the Settings dialog and the CLI set it,
    /// a change takes effect when the audio device restarts or on the next
    /// bounce. Defaults to Signalsmith.
    StretchEngine getSelected();

    /// Returns false (and keeps the selection) for an engine this build lacks.
    bool setSelected (StretchEngine engine);

    /// A fresh, unprepared backend; falls back to Signalsmith for an engine
    /// this build lacks.
    std::unique_ptr<StretchBackend> create (StretchEngine engine);
}

} // namespace audium
