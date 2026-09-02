//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <functional>
#include <string>
#include <vector>

// The demucs.cpp inference library, behind an interface that exposes neither
// Eigen nor demucs.cpp itself.
//
// demucs.cpp needs an Eigen newer than the one Essentia's tree carries, and
// the two cannot share a translation unit. So this header is deliberately
// plain C++ and the implementation is the only Audionaut source compiled
// against demucs.cpp's vendored Eigen (see the demucs flag scheme in the
// .jucer and the audionaut_demucs target in Catch2Tests/CMakeLists.txt). Keep
// it that way: nothing here may include JUCE or Eigen.
namespace audium::demucs {

/// Demucs models only work at this rate; the caller resamples.
constexpr int sampleRate = 44100;

/// The stems of the htdemucs 4-source model, in the model's output order.
constexpr int numSources = 4;

/**
 * Progress notifications from a separation run.
 *
 * @param fraction  0..1 across model loading and inference.
 * @param message   What is currently happening, for a status line.
 * @return          false to cancel; the run then returns false with an empty
 *                  error and no output.
 */
using ProgressFn = std::function<bool (float fraction, const std::string& message)>;

/**
 * Separates a stereo mix into the model's sources.
 *
 * The audio is split into one segment per thread - overlapping, crossfaded
 * back together - and each segment runs through demucs.cpp's own 7.8-second
 * windowed inference on its own thread. This is the demucs.cpp "_mt" driver
 * with progress aggregated into one callback and cancellation added.
 *
 * Cancellation is checked between demucs.cpp's inference windows, so it can
 * take a few seconds to take effect.
 *
 * @param modelFile   Path to a ggml htdemucs 4-source weights file.
 * @param left        Left channel, numSamples samples at sampleRate.
 * @param right       Right channel, numSamples samples at sampleRate.
 * @param numSamples  Samples per channel.
 * @param numThreads  Segments to process in parallel (clamped to what the
 *                    length allows; at least 1).
 * @param progress    Progress callback; may be empty.
 * @param stems       Receives numSources * 2 buffers of numSamples each,
 *                    indexed [source * 2 + channel].
 * @param error       Set on failure; left empty on cancellation.
 * @return            true on success.
 */
bool separate (const std::string& modelFile,
               const float* left,
               const float* right,
               size_t numSamples,
               int numThreads,
               const ProgressFn& progress,
               std::vector<std::vector<float>>& stems,
               std::string& error);

} // namespace audium::demucs
