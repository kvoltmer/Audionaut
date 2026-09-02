//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "DemucsRunner.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <exception>
#include <memory>
#include <mutex>
#include <thread>

// demucs.cpp: compiled only in this translation unit, against its own Eigen.
#include <model.hpp>
#include <tensor.hpp>

namespace audium::demucs {

namespace {

/// Thrown out of demucs.cpp's progress callback to abandon a segment: the
/// library has no cancellation hook of its own.
struct Cancelled {};

/// Context handed to adjacent segments on both sides, so the crossfade
/// between them has settled model output to work with. demucs.cpp's threaded
/// driver uses 0.75 s; kept.
constexpr int overlapSamples = static_cast<int> (0.75 * sampleRate);

struct ThreadState
{
    std::atomic<float> progress { 0.0f };
    std::atomic<bool> finished { false };
};

/// Copies `count` samples of `source` starting at `from` into `target` at
/// `to`, dropping whatever falls outside `source`. The dropped part stays
/// zero, which is what the driver wants at the ends of the audio.
void copyClamped (Eigen::MatrixXf& target, int to,
                  const Eigen::MatrixXf& source, int from, int count)
{
    const auto total = static_cast<int> (source.cols());
    const auto first = std::max (from, 0);
    const auto last  = std::min (from + count, total);

    if (last <= first)
        return;

    target.block (0, to + (first - from), 2, last - first) = source.block (0, first, 2, last - first);
}

/**
 * The segmented, multithreaded driver.
 *
 * Splits the audio into numThreads consecutive segments, each padded with
 * overlapSamples of its neighbours on both sides, runs demucs.cpp on every
 * segment on its own thread, and recombines the outputs with a linear
 * crossfade over the doubled overlap region between neighbours.
 */
bool runThreaded (const demucscpp::demucs_model& model,
                  const Eigen::MatrixXf& audio,
                  int numThreads,
                  const ProgressFn& progress,
                  float progressOffset,
                  Eigen::Tensor3dXf& output,
                  std::string& error)
{
    const auto totalLength = static_cast<int> (audio.cols());

    // A segment must be at least as long as the crossfade on each side, or
    // the ramps of one segment would overlap each other.
    const auto maxThreads = std::max (1, totalLength / (2 * overlapSamples));
    numThreads = std::clamp (numThreads, 1, maxThreads);

    const auto segmentLength = (totalLength + numThreads - 1) / numThreads;
    const auto paddedLength  = segmentLength + 2 * overlapSamples;

    std::vector<Eigen::MatrixXf> segments;
    segments.reserve (static_cast<size_t> (numThreads));

    for (auto i = 0; i < numThreads; ++i)
    {
        const auto start = i * segmentLength;
        Eigen::MatrixXf segment = Eigen::MatrixXf::Zero (2, paddedLength);
        copyClamped (segment, 0, audio, start - overlapSamples, paddedLength);
        segments.push_back (std::move (segment));
    }

    std::vector<Eigen::Tensor3dXf> segmentOutputs (static_cast<size_t> (numThreads));
    std::vector<std::unique_ptr<ThreadState>> states;
    for (auto i = 0; i < numThreads; ++i)
        states.push_back (std::make_unique<ThreadState>());

    std::atomic<bool> cancelled { false };
    std::mutex errorMutex;
    std::string threadError;

    std::vector<std::thread> threads;

    for (auto i = 0; i < numThreads; ++i)
    {
        threads.emplace_back ([&, i]
        {
            auto& state = *states[static_cast<size_t> (i)];

            demucscpp::ProgressCallback callback = [&state, &cancelled] (float fraction, const std::string&)
            {
                if (cancelled.load())
                    throw Cancelled {};

                state.progress.store (fraction);
            };

            try
            {
                segmentOutputs[static_cast<size_t> (i)] =
                    demucscpp::demucs_inference (model, segments[static_cast<size_t> (i)], callback);
                state.progress.store (1.0f);
            }
            catch (const Cancelled&)
            {
            }
            catch (const std::exception& e)
            {
                std::lock_guard<std::mutex> lock (errorMutex);
                if (threadError.empty())
                    threadError = e.what();
                cancelled.store (true);
            }
            catch (...)
            {
                std::lock_guard<std::mutex> lock (errorMutex);
                if (threadError.empty())
                    threadError = "separation failed";
                cancelled.store (true);
            }

            state.finished.store (true);
        });
    }

    // Report from this thread only: the per-segment callbacks fire
    // concurrently and the caller's callback must not.
    auto allFinished = [&states]
    {
        return std::all_of (states.begin(), states.end(),
                            [] (const auto& s) { return s->finished.load(); });
    };

    while (! allFinished())
    {
        std::this_thread::sleep_for (std::chrono::milliseconds (100));

        float sum = 0.0f;
        for (const auto& state : states)
            sum += state->progress.load();

        const auto fraction = progressOffset + (1.0f - progressOffset) * (sum / static_cast<float> (numThreads));

        if (progress != nullptr && ! cancelled.load() && ! progress (fraction, "Separating stems"))
            cancelled.store (true);
    }

    for (auto& thread : threads)
        thread.join();

    if (! threadError.empty())
    {
        error = threadError;
        return false;
    }

    if (cancelled.load())
        return false;

    const auto numOutputSources = model.is_4sources ? 4 : 6;

    output = Eigen::Tensor3dXf (numOutputSources, 2, totalLength);
    output.setZero();

    Eigen::VectorXf weightSum = Eigen::VectorXf::Zero (totalLength);

    // Neighbouring segments overlap by 2 * overlapSamples: one segment's
    // trailing padding plus the next one's leading padding. Fade the earlier
    // one out and the later one in across that whole region.
    const auto rampLength = 2 * overlapSamples;

    for (auto i = 0; i < numThreads; ++i)
    {
        const auto& segmentOutput = segmentOutputs[static_cast<size_t> (i)];
        const auto segmentStart   = i * segmentLength - overlapSamples;
        const auto segmentSamples = static_cast<int> (segmentOutput.dimension (2));

        for (auto j = 0; j < segmentSamples; ++j)
        {
            const auto globalIndex = segmentStart + j;

            if (globalIndex < 0 || globalIndex >= totalLength)
                continue;

            auto weight = 1.0f;

            if (i > 0 && j < rampLength)
                weight = static_cast<float> (j + 1) / static_cast<float> (rampLength);
            else if (i < numThreads - 1 && j >= segmentSamples - rampLength)
                weight = static_cast<float> (segmentSamples - j) / static_cast<float> (rampLength);

            for (auto source = 0; source < numOutputSources; ++source)
                for (auto channel = 0; channel < 2; ++channel)
                    output (source, channel, globalIndex) += segmentOutput (source, channel, j) * weight;

            weightSum (globalIndex) += weight;
        }
    }

    for (auto index = 0; index < totalLength; ++index)
    {
        const auto sum = weightSum (index);

        if (sum <= 0.0f)
            continue;

        for (auto source = 0; source < numOutputSources; ++source)
            for (auto channel = 0; channel < 2; ++channel)
                output (source, channel, index) /= sum;
    }

    return true;
}

} // namespace

bool separate (const std::string& modelFile,
               const float* left,
               const float* right,
               size_t numSamples,
               int numThreads,
               const ProgressFn& progress,
               std::vector<std::vector<float>>& stems,
               std::string& error)
{
    stems.clear();

    if (left == nullptr || right == nullptr || numSamples == 0)
    {
        error = "no audio to separate";
        return false;
    }

    if (progress != nullptr && ! progress (0.0f, "Loading model"))
        return false;

    // The model struct holds every weight tensor; far too large for the stack.
    auto model = std::make_unique<demucscpp::demucs_model>();

    if (! demucscpp::load_demucs_model (modelFile, model.get()))
    {
        error = "could not load the separation model from " + modelFile;
        return false;
    }

    if (! model->is_4sources)
    {
        error = "unsupported model: only the 4-source htdemucs model is supported";
        return false;
    }

    // Loading is a few seconds of a minutes-long job.
    constexpr auto loadingShare = 0.03f;

    if (progress != nullptr && ! progress (loadingShare, "Separating stems"))
        return false;

    Eigen::MatrixXf audio (2, static_cast<Eigen::Index> (numSamples));

    for (size_t i = 0; i < numSamples; ++i)
    {
        audio (0, static_cast<Eigen::Index> (i)) = left[i];
        audio (1, static_cast<Eigen::Index> (i)) = right[i];
    }

    Eigen::Tensor3dXf output;

    if (! runThreaded (*model, audio, numThreads, progress, loadingShare, output, error))
        return false;

    model.reset();

    const auto numOutputSources = static_cast<int> (output.dimension (0));
    stems.resize (static_cast<size_t> (numOutputSources * 2));

    for (auto source = 0; source < numOutputSources; ++source)
    {
        for (auto channel = 0; channel < 2; ++channel)
        {
            auto& buffer = stems[static_cast<size_t> (source * 2 + channel)];
            buffer.resize (numSamples);

            for (size_t i = 0; i < numSamples; ++i)
                buffer[i] = output (source, channel, static_cast<Eigen::Index> (i));
        }
    }

    if (progress != nullptr)
        progress (1.0f, "Done");

    return true;
}

} // namespace audium::demucs
