//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <cmath>

#include "EventMerger.h"

namespace audium {

namespace {

// True when the parameters describe a grid that can actually be converted on.
bool hasUsableGrid (const EventMerger::Parameters& params)
{
    return params.hopSize > 0 && params.gridRate > 0.0f;
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

    // The merge itself lands in the following phases of this port, each on top
    // of the contract above:
    //   phase 2 - stage A, mean intervals and the activation matrix
    //   phase 3 - stage B, Ricker kernels, convolution and peak picking
    //   phase 4 - stage C, the minimum-length pass and the conversion back to
    //             seconds
    // Until then a well-formed call returns no boundaries rather than wrong
    // ones.
    juce::ignoreUnused (numFrames);

    return result;
}

} // namespace audium
