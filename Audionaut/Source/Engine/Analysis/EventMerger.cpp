//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <algorithm>
#include <cmath>
#include <numeric>
#include <utility>

#include "EventMerger.h"

namespace audium {

namespace {

// True when the parameters describe a grid that can actually be converted on.
bool hasUsableGrid (const EventMerger::Parameters& params)
{
    return params.hopSize > 0 && params.gridRate > 0.0f;
}

// A stream needs two events before an interval can be derived from it.
constexpr size_t minEventsPerStream = 2;

// Quantises a stream's event times onto the frame grid. Out-of-range frames are
// kept here and discarded when the column is filled, so that dropping them
// cannot change the interval.
std::vector<int> toFrames (const std::vector<float>& times,
                           const EventMerger::Parameters& params)
{
    std::vector<int> frames;
    frames.reserve (times.size());

    for (auto time : times)
        frames.push_back (EventMerger::timeToFrame (time, params));

    return frames;
}

// Mean gap between consecutive events, in frames. The sum of the differences
// telescopes, so this is the mean of the diffs the reference computes.
// Caller guarantees at least minEventsPerStream entries.
float meanInterval (const std::vector<int>& frames)
{
    jassert (frames.size() >= minEventsPerStream);

    const auto span = (double) frames.back() - (double) frames.front();
    return (float) (span / (double) (frames.size() - 1));
}

// An activation column carrying a 1 on each of the first `count` frames of the
// stream that lands inside the grid.
std::vector<float> makeColumn (const std::vector<int>& frames,
                               size_t count,
                               int numFrames)
{
    std::vector<float> column ((size_t) numFrames, 0.0f);

    for (size_t i = 0; i < count; ++i)
        if (frames[i] >= 0 && frames[i] < numFrames)
            column[(size_t) frames[i]] = 1.0f;

    return column;
}

} // namespace

int EventMerger::timeToFrame (float seconds, const Parameters& params)
{
    if (! hasUsableGrid (params))
        return 0;

    const auto frames = (double) seconds * (double) params.gridRate / (double) params.hopSize;
    return (int) std::lround (frames);
}

float EventMerger::frameToTime (int frame, const Parameters& params)
{
    if (! hasUsableGrid (params))
        return 0.0f;

    return (float) ((double) frame * (double) params.hopSize / (double) params.gridRate);
}

int EventMerger::frameCount (float durationSeconds, const Parameters& params)
{
    if (! hasUsableGrid (params) || durationSeconds <= 0.0f)
        return 0;

    const auto frames = (double) durationSeconds * (double) params.gridRate / (double) params.hopSize;
    return (int) std::floor (frames);
}

EventMerger::Activations EventMerger::buildActivations (const std::vector<EventStream>& streams,
                                                        int numFrames,
                                                        const Parameters& params)
{
    Activations activations;

    if (numFrames < 1)
        return activations;

    activations.numFrames = numFrames;

    // Split by kind, preserving order, and quantise up front. The reference
    // keeps segmentation and beat events in separate lists and treats them
    // differently below.
    std::vector<const EventStream*> segStreams, beatStreams;
    std::vector<std::vector<int>> segFrames, beatFrames;

    for (const auto& stream : streams)
    {
        auto frames = toFrames (stream.times, params);

        if (stream.kind == Kind::Segmentation)
        {
            segStreams.push_back (&stream);
            segFrames.push_back (std::move (frames));
        }
        else
        {
            beatStreams.push_back (&stream);
            beatFrames.push_back (std::move (frames));
        }
    }

    const auto validIndicesOf = [] (const std::vector<std::vector<int>>& frames)
    {
        std::vector<size_t> valid;

        for (size_t i = 0; i < frames.size(); ++i)
            if (frames[i].size() >= minEventsPerStream)
                valid.push_back (i);

        return valid;
    };

    const auto validSegs = validIndicesOf (segFrames);
    const auto validBeats = validIndicesOf (beatFrames);

    activations.intervals.reserve (validSegs.size() + validBeats.size());
    activations.columns.reserve (validSegs.size() + validBeats.size());
    activations.labels.reserve (validSegs.size() + validBeats.size());

    // Segmentation columns first, matching the reference's column order.
    //
    // The reference guards only its beat intervals against a too-short stream,
    // so a segmentation stream with a single boundary yields a NaN interval
    // there, which then poisons the whole summed activation. We skip such
    // streams instead, exactly as the beat path already does. No parity is lost
    // because the reference's behaviour in that case is unusable either way.
    for (auto segIndex : validSegs)
    {
        const auto& frames = segFrames[segIndex];

        // Taken from every event, including the one dropped just below - the
        // reference computes its intervals before truncating.
        activations.intervals.push_back (meanInterval (frames));
        activations.labels.push_back (segStreams[segIndex]->label);

        auto count = frames.size();

        if (params.dropLastSegBoundary)
            --count;

        activations.columns.push_back (makeColumn (frames, count, numFrames));
    }

    // Beat columns. Note the asymmetry the reference introduces here: the i-th
    // beat column's interval comes from the i-th *valid* beat stream, but its
    // events are taken from the i-th beat stream overall. The two coincide
    // whenever every beat stream is valid; strictStreamMapping pairs each
    // column with the stream its interval actually came from.
    for (size_t i = 0; i < validBeats.size(); ++i)
    {
        activations.intervals.push_back (meanInterval (beatFrames[validBeats[i]]));

        const auto eventIndex = params.strictStreamMapping ? validBeats[i] : i;

        activations.labels.push_back (beatStreams[eventIndex]->label);
        activations.columns.push_back (makeColumn (beatFrames[eventIndex],
                                                   beatFrames[eventIndex].size(),
                                                   numFrames));
    }

    return activations;
}

std::vector<float> EventMerger::ricker (int points, float width)
{
    std::vector<float> kernel;

    if (points < 1 || width <= 0.0f)
        return kernel;

    const auto a = (double) width;
    const auto wsq = a * a;

    // 2 / (sqrt(3a) * pi^(1/4))
    const auto amplitude = 2.0 / (std::sqrt (3.0 * a) * std::pow (juce::MathConstants<double>::pi, 0.25));

    // x runs symmetrically about the centre, so an even-length kernel straddles
    // the middle rather than sitting on it.
    const auto centre = ((double) points - 1.0) / 2.0;

    kernel.reserve ((size_t) points);

    for (auto i = 0; i < points; ++i)
    {
        const auto x = (double) i - centre;
        const auto xsq = x * x;

        kernel.push_back ((float) (amplitude * (1.0 - xsq / wsq) * std::exp (-xsq / (2.0 * wsq))));
    }

    return kernel;
}

std::vector<float> EventMerger::convolveSame (const std::vector<float>& signal,
                                              const std::vector<float>& kernel)
{
    std::vector<float> output;

    if (signal.empty() || kernel.empty())
        return output;

    const auto signalLength = (int) signal.size();
    const auto kernelLength = (int) kernel.size();

    // numpy convolves the longer input with the shorter one and keeps the
    // centre max(M, N) samples of the full result, so the offset into that
    // result is derived from the shorter length.
    const auto outputLength = std::max (signalLength, kernelLength);
    const auto shorter = std::min (signalLength, kernelLength);
    const auto offset = (shorter - 1) / 2;

    output.reserve ((size_t) outputLength);

    for (auto i = 0; i < outputLength; ++i)
    {
        // full[n] = sum over k of signal[k] * kernel[n - k]
        const auto n = i + offset;

        // Clamp k to where both inputs are in range, rather than testing inside
        // the loop.
        const auto firstK = std::max (0, n - kernelLength + 1);
        const auto lastK = std::min (n, signalLength - 1);

        auto sum = 0.0;

        for (auto k = firstK; k <= lastK; ++k)
            sum += (double) signal[(size_t) k] * (double) kernel[(size_t) (n - k)];

        output.push_back ((float) sum);
    }

    return output;
}

std::vector<float> EventMerger::summedActivation (const Activations& activations,
                                                  const Parameters& params)
{
    std::vector<float> summed;

    if (activations.columns.empty() || activations.numFrames < 1)
        return summed;

    jassert (activations.columns.size() == activations.intervals.size());

    // The reference caps the kernel at 250 points and shortens it further when
    // the material itself is shorter.
    const auto points = std::min (params.maxKernelPoints, activations.numFrames);

    if (points < 1)
        return summed;

    summed.assign ((size_t) activations.numFrames, 0.0f);

    for (size_t i = 0; i < activations.columns.size(); ++i)
    {
        const auto width = activations.intervals[i] / params.kernelWidthDivisor;

        // No usable width - every event of the stream landed on one frame, or
        // the divisor is degenerate. Contribute nothing rather than NaN.
        if (! (width > 0.0f))
            continue;

        const auto kernel = ricker (points, width);
        const auto convolved = convolveSame (activations.columns[i], kernel);

        if (convolved.size() != summed.size())
        {
            jassertfalse;
            continue;
        }

        // The reference computes these weights and then never applies them.
        const auto weight = params.applyKernelWeights ? 1.0f + (float) i : 1.0f;

        for (size_t frame = 0; frame < summed.size(); ++frame)
            summed[frame] += convolved[frame] * weight;
    }

    return summed;
}

std::vector<int> EventMerger::pickPeaks (const std::vector<float>& activation,
                                         int numSegments,
                                         const Parameters& params)
{
    std::vector<int> picked;

    if (activation.empty() || numSegments < 1)
        return picked;

    // The reference passes numsegs straight to np.argpartition, which raises
    // when it exceeds the frame count.
    const auto wanted = (size_t) std::min ((size_t) numSegments, activation.size());

    std::vector<int> order ((size_t) activation.size());
    std::iota (order.begin(), order.end(), 0);

    // Strongest first, ties by frame order so the selection is deterministic.
    const auto stronger = [&activation] (int lhs, int rhs)
    {
        const auto a = activation[(size_t) lhs];
        const auto b = activation[(size_t) rhs];

        if (a != b)
            return a > b;

        return lhs < rhs;
    };

    std::partial_sort (order.begin(), order.begin() + (long) wanted, order.end(), stronger);

    picked.assign (order.begin(), order.begin() + (long) wanted);

    for (auto& index : picked)
        index += params.peakIndexOffset;

    std::sort (picked.begin(), picked.end());

    return picked;
}

EventMerger::Result EventMerger::merge (const std::vector<EventStream>& streams,
                                        float durationSeconds)
{
    return merge (streams, durationSeconds, Parameters());
}

EventMerger::Result EventMerger::merge (const std::vector<EventStream>& streams,
                                        float durationSeconds,
                                        const Parameters& params)
{
    Result result;

    // Degenerate input: nothing to merge, no material to merge over, or no
    // segments asked for.
    if (streams.empty() || params.numSegments < 1)
        return result;

    const auto numFrames = frameCount (durationSeconds, params);

    if (numFrames < 1)
        return result;

    // Stage A: one activation column per contributing stream.
    const auto activations = buildActivations (streams, numFrames, params);

    if (activations.columns.empty())
        return result;

    // Stage B: kernel per column, summed, strongest peaks kept.
    auto summed = summedActivation (activations, params);

    if (summed.empty())
        return result;

    const auto peaks = pickPeaks (summed, params.numSegments, params);

    // Stage C - the minimum-length pass, and prefixing the run with a boundary
    // at zero - lands in the next phase of this port. Until then the peaks are
    // returned as they were picked, less any the index offset pushed off the
    // grid.
    for (auto peak : peaks)
        if (peak >= 0 && peak < numFrames)
            result.boundaries.push_back (frameToTime (peak, params));

    result.activation = std::move (summed);

    return result;
}

} // namespace audium
