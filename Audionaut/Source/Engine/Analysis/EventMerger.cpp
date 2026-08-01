//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <cmath>
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

    // The remaining stages land in the following phases of this port:
    //   phase 3 - stage B, Ricker kernels, convolution and peak picking
    //   phase 4 - stage C, the minimum-length pass and the conversion back to
    //             seconds
    // Until then a well-formed call returns no boundaries rather than wrong
    // ones.
    return result;
}

} // namespace audium
