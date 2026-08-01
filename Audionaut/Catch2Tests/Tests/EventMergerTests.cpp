#include <cmath>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Engine/Analysis/EventMerger.h"

// EventMerger is pure arithmetic and does not link against Essentia, so unlike
// the segmenter scenarios these run unconditionally.

using namespace audium;

namespace {

// A stream of evenly spaced events, which is what the merge is fed in practice.
EventMerger::EventStream makeStream (const std::string& label,
                                     EventMerger::Kind kind,
                                     float firstTime,
                                     float interval,
                                     int count)
{
    EventMerger::EventStream stream;
    stream.label = label;
    stream.kind = kind;

    for (auto i = 0; i < count; ++i)
        stream.times.push_back (firstTime + interval * (float) i);

    return stream;
}

// A stream whose events sit exactly on the given grid frames, so expectations
// can be written in frame terms.
EventMerger::EventStream streamFromFrames (const std::string& label,
                                           EventMerger::Kind kind,
                                           const std::vector<int>& frames,
                                           const EventMerger::Parameters& params)
{
    EventMerger::EventStream stream;
    stream.label = label;
    stream.kind = kind;

    for (auto frame : frames)
        stream.times.push_back (EventMerger::frameToTime (frame, params));

    return stream;
}

// The frames a column carries an activation on.
std::vector<int> activeFrames (const std::vector<float>& column)
{
    std::vector<int> frames;

    for (size_t i = 0; i < column.size(); ++i)
        if (column[i] > 0.0f)
            frames.push_back ((int) i);

    return frames;
}

} // namespace

SCENARIO("EventMerger converts between seconds and its internal frame grid",
         "[engine][analysis][merge]")
{
    EventMerger::Parameters params;

    GIVEN("the default grid of hop 512 at 22050 Hz")
    {
        REQUIRE(params.hopSize == 512);
        REQUIRE(params.gridRate == Catch::Approx(22050.0f));

        WHEN("a frame is converted to seconds")
        {
            THEN("it lands at hopSize/gridRate per frame")
            {
                REQUIRE(EventMerger::frameToTime (0, params) == Catch::Approx(0.0f));
                REQUIRE(EventMerger::frameToTime (1, params) == Catch::Approx(512.0f / 22050.0f));
                // The reference's hardcoded minimum segment length, ~1 second.
                REQUIRE(EventMerger::frameToTime (43, params) == Catch::Approx(0.99846f).margin(0.0001f));
            }
        }

        WHEN("a time is converted to a frame")
        {
            THEN("it rounds to the nearest frame")
            {
                REQUIRE(EventMerger::timeToFrame (0.0f, params) == 0);
                // 1 s is 43.066 frames on this grid.
                REQUIRE(EventMerger::timeToFrame (1.0f, params) == 43);
                REQUIRE(EventMerger::timeToFrame (10.0f, params) == 431);
            }
        }

        WHEN("a frame is converted to seconds and back")
        {
            THEN("the original frame is recovered exactly")
            {
                // Rounding rather than truncating is what makes this hold; it
                // matters because every input event originates as a frame index
                // in an upstream analysis.
                for (auto frame : { 0, 1, 2, 43, 100, 431, 1000, 12345 })
                    REQUIRE(EventMerger::timeToFrame (EventMerger::frameToTime (frame, params), params) == frame);
            }
        }

        WHEN("a duration is converted to a frame count")
        {
            THEN("it truncates, matching librosa's samples_to_frames")
            {
                // 10 s is 430.66 frames, so 430 whole frames.
                REQUIRE(EventMerger::frameCount (10.0f, params) == 430);
                REQUIRE(EventMerger::frameCount (1.0f, params) == 43);
            }

            THEN("a non-positive duration spans no frames")
            {
                REQUIRE(EventMerger::frameCount (0.0f, params) == 0);
                REQUIRE(EventMerger::frameCount (-5.0f, params) == 0);
            }
        }
    }

    GIVEN("parameters describing no usable grid")
    {
        WHEN("the hop size is zero")
        {
            params.hopSize = 0;

            THEN("the conversions return zero rather than dividing by it")
            {
                REQUIRE(EventMerger::timeToFrame (1.0f, params) == 0);
                REQUIRE(EventMerger::frameToTime (43, params) == Catch::Approx(0.0f));
                REQUIRE(EventMerger::frameCount (10.0f, params) == 0);
            }
        }

        WHEN("the grid rate is zero")
        {
            params.gridRate = 0.0f;

            THEN("the conversions return zero")
            {
                REQUIRE(EventMerger::timeToFrame (1.0f, params) == 0);
                REQUIRE(EventMerger::frameToTime (43, params) == Catch::Approx(0.0f));
                REQUIRE(EventMerger::frameCount (10.0f, params) == 0);
            }
        }
    }
}

SCENARIO("EventMerger rejects degenerate input", "[engine][analysis][merge]")
{
    EventMerger merger;

    // One segmentation and one beat stream over 10 seconds of material.
    const std::vector<EventMerger::EventStream> streams {
        makeStream ("sbic", EventMerger::Kind::Segmentation, 0.0f, 2.0f, 5),
        makeStream ("beats", EventMerger::Kind::Beat, 0.0f, 0.5f, 20)
    };

    GIVEN("no streams to merge")
    {
        WHEN("a merge is requested")
        {
            auto result = merger.merge ({}, 10.0f);

            THEN("it yields nothing, without crashing")
            {
                REQUIRE(result.boundaries.empty());
                REQUIRE(result.activation.empty());
            }
        }
    }

    GIVEN("streams but no material")
    {
        WHEN("the duration is zero")
        {
            auto result = merger.merge (streams, 0.0f);

            THEN("it yields nothing")
            {
                REQUIRE(result.boundaries.empty());
                REQUIRE(result.activation.empty());
            }
        }

        WHEN("the duration is negative")
        {
            auto result = merger.merge (streams, -10.0f);

            THEN("it yields nothing")
            {
                REQUIRE(result.boundaries.empty());
                REQUIRE(result.activation.empty());
            }
        }

        WHEN("the duration is shorter than a single grid frame")
        {
            auto result = merger.merge (streams, 0.001f);

            THEN("it yields nothing")
            {
                REQUIRE(result.boundaries.empty());
                REQUIRE(result.activation.empty());
            }
        }
    }

    GIVEN("fewer than one segment requested")
    {
        EventMerger::Parameters params;

        WHEN("zero segments are asked for")
        {
            params.numSegments = 0;
            auto result = merger.merge (streams, 10.0f, params);

            THEN("it yields nothing")
            {
                REQUIRE(result.boundaries.empty());
                REQUIRE(result.activation.empty());
            }
        }

        WHEN("a negative number of segments is asked for")
        {
            params.numSegments = -1;
            auto result = merger.merge (streams, 10.0f, params);

            THEN("it yields nothing")
            {
                REQUIRE(result.boundaries.empty());
                REQUIRE(result.activation.empty());
            }
        }
    }
}

SCENARIO("EventMerger builds one activation column per contributing stream",
         "[engine][analysis][merge]")
{
    EventMerger::Parameters params;
    const auto numFrames = 100;

    GIVEN("two segmentation and two beat streams")
    {
        const std::vector<EventMerger::EventStream> streams {
            streamFromFrames ("segA", EventMerger::Kind::Segmentation, { 0, 10, 20, 30 }, params),
            streamFromFrames ("segB", EventMerger::Kind::Segmentation, { 0, 25, 50 }, params),
            streamFromFrames ("beatA", EventMerger::Kind::Beat, { 0, 5, 10, 15, 20, 25 }, params),
            streamFromFrames ("beatB", EventMerger::Kind::Beat, { 0, 8, 16, 24 }, params)
        };

        WHEN("the activations are built")
        {
            auto activations = EventMerger::buildActivations (streams, numFrames, params);

            THEN("there is a column per stream, segmentation first")
            {
                REQUIRE(activations.numFrames == numFrames);
                REQUIRE(activations.columns.size() == 4);
                REQUIRE(activations.intervals.size() == 4);
                REQUIRE(activations.labels == std::vector<std::string> { "segA", "segB", "beatA", "beatB" });
            }

            THEN("every column spans the whole grid")
            {
                for (const auto& column : activations.columns)
                    REQUIRE(column.size() == (size_t) numFrames);
            }

            THEN("each interval is the mean gap of its stream")
            {
                REQUIRE(activations.intervals[0] == Catch::Approx(10.0f));
                REQUIRE(activations.intervals[1] == Catch::Approx(25.0f));
                REQUIRE(activations.intervals[2] == Catch::Approx(5.0f));
                REQUIRE(activations.intervals[3] == Catch::Approx(8.0f));
            }

            THEN("segmentation activations land on the stream's frames, less the last")
            {
                REQUIRE(activeFrames (activations.columns[0]) == std::vector<int> { 0, 10, 20 });
                REQUIRE(activeFrames (activations.columns[1]) == std::vector<int> { 0, 25 });
            }

            THEN("beat activations land on every frame of the stream")
            {
                REQUIRE(activeFrames (activations.columns[2]) == std::vector<int> { 0, 5, 10, 15, 20, 25 });
                REQUIRE(activeFrames (activations.columns[3]) == std::vector<int> { 0, 8, 16, 24 });
            }
        }
    }

    GIVEN("events beyond the end of the grid")
    {
        // Deliberately spaced so that discarding the out-of-range event would
        // change the mean gap: 130/3 across all four, but 40 across the three
        // that fit.
        const std::vector<EventMerger::EventStream> streams {
            streamFromFrames ("seg", EventMerger::Kind::Segmentation, { 0, 40, 80, 130 }, params)
        };

        WHEN("the activations are built")
        {
            params.dropLastSegBoundary = false;
            auto activations = EventMerger::buildActivations (streams, numFrames, params);

            THEN("out-of-range events are dropped from the column")
            {
                REQUIRE(activations.columns.size() == 1);
                REQUIRE(activeFrames (activations.columns[0]) == std::vector<int> { 0, 40, 80 });
            }

            THEN("but they still count towards the interval")
            {
                REQUIRE(activations.intervals[0] == Catch::Approx(130.0f / 3.0f));
            }
        }
    }
}

SCENARIO("EventMerger ignores streams too short to yield an interval",
         "[engine][analysis][merge]")
{
    EventMerger::Parameters params;
    const auto numFrames = 100;

    GIVEN("a beat stream carrying a single event")
    {
        const std::vector<EventMerger::EventStream> streams {
            streamFromFrames ("seg", EventMerger::Kind::Segmentation, { 0, 20, 40, 60 }, params),
            streamFromFrames ("beatShort", EventMerger::Kind::Beat, { 7 }, params),
            streamFromFrames ("beatLong", EventMerger::Kind::Beat, { 0, 4, 8, 12 }, params)
        };

        WHEN("the activations are built")
        {
            auto activations = EventMerger::buildActivations (streams, numFrames, params);

            THEN("it contributes neither a column nor an interval")
            {
                REQUIRE(activations.columns.size() == 2);
                REQUIRE(activations.intervals.size() == 2);
                REQUIRE(activations.intervals[0] == Catch::Approx(20.0f));
                REQUIRE(activations.intervals[1] == Catch::Approx(4.0f));
            }
        }
    }

    GIVEN("a segmentation stream carrying a single boundary")
    {
        const std::vector<EventMerger::EventStream> streams {
            streamFromFrames ("segShort", EventMerger::Kind::Segmentation, { 12 }, params),
            streamFromFrames ("segLong", EventMerger::Kind::Segmentation, { 0, 20, 40, 60 }, params),
            streamFromFrames ("beat", EventMerger::Kind::Beat, { 0, 4, 8, 12 }, params)
        };

        WHEN("the activations are built")
        {
            auto activations = EventMerger::buildActivations (streams, numFrames, params);

            THEN("it is skipped rather than yielding a NaN interval")
            {
                // The reference guards only its beat streams this way, so this
                // case produces a NaN there and poisons the summed activation.
                REQUIRE(activations.columns.size() == 2);
                REQUIRE(activations.labels == std::vector<std::string> { "segLong", "beat" });

                for (auto interval : activations.intervals)
                    REQUIRE(std::isfinite (interval));
            }
        }
    }

    GIVEN("no stream long enough to contribute")
    {
        const std::vector<EventMerger::EventStream> streams {
            streamFromFrames ("seg", EventMerger::Kind::Segmentation, { 5 }, params),
            streamFromFrames ("beat", EventMerger::Kind::Beat, {}, params)
        };

        WHEN("the activations are built")
        {
            auto activations = EventMerger::buildActivations (streams, numFrames, params);

            THEN("there is nothing to merge")
            {
                REQUIRE(activations.columns.empty());
                REQUIRE(activations.intervals.empty());
            }
        }
    }
}

SCENARIO("EventMerger's dropLastSegBoundary flag truncates only segmentation columns",
         "[engine][analysis][merge]")
{
    EventMerger::Parameters params;
    const auto numFrames = 100;

    const std::vector<EventMerger::EventStream> streams {
        streamFromFrames ("seg", EventMerger::Kind::Segmentation, { 0, 10, 20, 30 }, params),
        streamFromFrames ("beat", EventMerger::Kind::Beat, { 0, 5, 10, 15 }, params)
    };

    GIVEN("the reference default, which drops the last boundary")
    {
        REQUIRE(params.dropLastSegBoundary);

        auto activations = EventMerger::buildActivations (streams, numFrames, params);

        THEN("the segmentation column stops one event short")
        {
            REQUIRE(activeFrames (activations.columns[0]) == std::vector<int> { 0, 10, 20 });
        }

        THEN("the beat column is untouched")
        {
            REQUIRE(activeFrames (activations.columns[1]) == std::vector<int> { 0, 5, 10, 15 });
        }
    }

    GIVEN("the flag disabled")
    {
        params.dropLastSegBoundary = false;

        auto activations = EventMerger::buildActivations (streams, numFrames, params);

        THEN("the segmentation column keeps every boundary")
        {
            REQUIRE(activeFrames (activations.columns[0]) == std::vector<int> { 0, 10, 20, 30 });
        }

        THEN("the beat column is still untouched")
        {
            REQUIRE(activeFrames (activations.columns[1]) == std::vector<int> { 0, 5, 10, 15 });
        }
    }

    GIVEN("either setting")
    {
        auto dropped = EventMerger::buildActivations (streams, numFrames, params);

        params.dropLastSegBoundary = false;
        auto kept = EventMerger::buildActivations (streams, numFrames, params);

        THEN("the intervals are identical, being taken before the truncation")
        {
            REQUIRE(dropped.intervals == kept.intervals);
        }
    }
}

SCENARIO("EventMerger's strictStreamMapping flag repairs the beat column pairing",
         "[engine][analysis][merge]")
{
    EventMerger::Parameters params;
    const auto numFrames = 100;

    GIVEN("beat streams that are all long enough")
    {
        const std::vector<EventMerger::EventStream> streams {
            streamFromFrames ("seg", EventMerger::Kind::Segmentation, { 0, 20, 40 }, params),
            streamFromFrames ("beatA", EventMerger::Kind::Beat, { 0, 4, 8, 12 }, params),
            streamFromFrames ("beatB", EventMerger::Kind::Beat, { 0, 6, 12, 18 }, params)
        };

        WHEN("the two mappings are compared")
        {
            auto loose = EventMerger::buildActivations (streams, numFrames, params);

            params.strictStreamMapping = true;
            auto strict = EventMerger::buildActivations (streams, numFrames, params);

            THEN("they agree exactly")
            {
                REQUIRE(loose.labels == strict.labels);
                REQUIRE(loose.intervals == strict.intervals);
                REQUIRE(loose.columns == strict.columns);
            }
        }
    }

    GIVEN("a beat stream too short to contribute an interval")
    {
        const std::vector<EventMerger::EventStream> streams {
            streamFromFrames ("seg", EventMerger::Kind::Segmentation, { 0, 20, 40 }, params),
            streamFromFrames ("beatShort", EventMerger::Kind::Beat, { 3 }, params),
            streamFromFrames ("beatB", EventMerger::Kind::Beat, { 0, 6, 12, 18 }, params),
            streamFromFrames ("beatC", EventMerger::Kind::Beat, { 0, 9, 18, 27 }, params)
        };

        WHEN("the reference mapping is used")
        {
            auto activations = EventMerger::buildActivations (streams, numFrames, params);

            THEN("columns take their events from the wrong streams")
            {
                // Both intervals come from the valid streams, beatB and beatC,
                // but the events come from the first two streams in order -
                // including the short one that contributed no interval at all.
                REQUIRE(activations.labels == std::vector<std::string> { "seg", "beatShort", "beatB" });
                REQUIRE(activations.intervals[1] == Catch::Approx(6.0f));
                REQUIRE(activations.intervals[2] == Catch::Approx(9.0f));
                REQUIRE(activeFrames (activations.columns[1]) == std::vector<int> { 3 });
            }
        }

        WHEN("strict mapping is used")
        {
            params.strictStreamMapping = true;
            auto activations = EventMerger::buildActivations (streams, numFrames, params);

            THEN("each column takes its events from the stream its interval came from")
            {
                REQUIRE(activations.labels == std::vector<std::string> { "seg", "beatB", "beatC" });
                REQUIRE(activations.intervals[1] == Catch::Approx(6.0f));
                REQUIRE(activations.intervals[2] == Catch::Approx(9.0f));
                REQUIRE(activeFrames (activations.columns[1]) == std::vector<int> { 0, 6, 12, 18 });
                REQUIRE(activeFrames (activations.columns[2]) == std::vector<int> { 0, 9, 18, 27 });
            }
        }
    }
}

SCENARIO("EventMerger's Ricker kernel matches the reference wavelet",
         "[engine][analysis][merge]")
{
    GIVEN("a five-point kernel of width one")
    {
        auto kernel = EventMerger::ricker (5, 1.0f);

        THEN("it matches scipy.signal.ricker's closed form")
        {
            // A = 2 / (sqrt(3) * pi^(1/4)) = 0.8673251, so the centre is A, the
            // wavelet crosses zero at |x| = a, and the tails are
            // A * (1 - 4) * exp(-2) = -0.3521391.
            REQUIRE(kernel.size() == 5);
            REQUIRE(kernel[0] == Catch::Approx(-0.3521391f).margin(1e-6f));
            REQUIRE(kernel[1] == Catch::Approx(0.0f).margin(1e-6f));
            REQUIRE(kernel[2] == Catch::Approx(0.8673251f).margin(1e-6f));
            REQUIRE(kernel[3] == Catch::Approx(0.0f).margin(1e-6f));
            REQUIRE(kernel[4] == Catch::Approx(-0.3521391f).margin(1e-6f));
        }
    }

    GIVEN("an odd-length kernel")
    {
        auto kernel = EventMerger::ricker (51, 4.0f);

        THEN("it is symmetric about its centre")
        {
            for (size_t i = 0; i < kernel.size() / 2; ++i)
                REQUIRE(kernel[i] == Catch::Approx(kernel[kernel.size() - 1 - i]).margin(1e-6f));
        }

        THEN("it peaks at the centre")
        {
            const auto peak = std::max_element (kernel.begin(), kernel.end());
            REQUIRE(std::distance (kernel.begin(), peak) == 25);
        }

        THEN("a wider kernel has a lower peak, spreading the same event further")
        {
            auto wider = EventMerger::ricker (51, 8.0f);
            REQUIRE(wider[25] < kernel[25]);
        }
    }

    GIVEN("degenerate arguments")
    {
        THEN("no kernel is produced")
        {
            REQUIRE(EventMerger::ricker (0, 1.0f).empty());
            REQUIRE(EventMerger::ricker (-5, 1.0f).empty());
            REQUIRE(EventMerger::ricker (5, 0.0f).empty());
            REQUIRE(EventMerger::ricker (5, -1.0f).empty());
        }
    }
}

SCENARIO("EventMerger's convolution keeps the same window numpy does",
         "[engine][analysis][merge]")
{
    // This scenario is the contract for the centring convention. numpy's
    // mode='same' keeps the centre max(M, N) samples of the full convolution,
    // starting at offset (K - 1) / 2 where K is the shorter input's length. Get
    // that offset wrong and every boundary shifts.

    GIVEN("an even-length kernel")
    {
        auto result = EventMerger::convolveSame ({ 1.0f, 2.0f, 3.0f, 4.0f }, { 1.0f, 1.0f });

        THEN("the window starts at the head of the full convolution")
        {
            // full = [1, 3, 5, 7, 4], offset = 0
            REQUIRE(result == std::vector<float> { 1.0f, 3.0f, 5.0f, 7.0f });
        }
    }

    GIVEN("a centred unit impulse as the kernel")
    {
        auto result = EventMerger::convolveSame ({ 1.0f, 2.0f, 3.0f }, { 0.0f, 1.0f, 0.0f });

        THEN("the signal comes back unchanged")
        {
            REQUIRE(result == std::vector<float> { 1.0f, 2.0f, 3.0f });
        }
    }

    GIVEN("an impulse at the very start of the signal")
    {
        auto result = EventMerger::convolveSame ({ 1.0f, 0.0f, 0.0f, 0.0f, 0.0f },
                                                 { 1.0f, 2.0f, 3.0f });

        THEN("the kernel is centred on that frame and clipped on the left")
        {
            // full = [1, 2, 3, 0, 0, 0, 0], offset = 1, so the kernel's leading
            // sample falls off the front.
            REQUIRE(result == std::vector<float> { 2.0f, 3.0f, 0.0f, 0.0f, 0.0f });
        }
    }

    GIVEN("the output length")
    {
        THEN("it is max(M, N)")
        {
            REQUIRE(EventMerger::convolveSame (std::vector<float> (10, 1.0f),
                                               std::vector<float> (3, 1.0f)).size() == 10);
            REQUIRE(EventMerger::convolveSame (std::vector<float> (4, 1.0f),
                                               std::vector<float> (4, 1.0f)).size() == 4);
        }
    }

    GIVEN("an empty input")
    {
        THEN("nothing is produced")
        {
            REQUIRE(EventMerger::convolveSame ({}, { 1.0f }).empty());
            REQUIRE(EventMerger::convolveSame ({ 1.0f }, {}).empty());
        }
    }
}

SCENARIO("EventMerger sums the kernel activations across streams",
         "[engine][analysis][merge]")
{
    EventMerger::Parameters params;
    const auto numFrames = 200;

    const std::vector<EventMerger::EventStream> streams {
        streamFromFrames ("seg", EventMerger::Kind::Segmentation, { 0, 60, 120, 180 }, params),
        streamFromFrames ("beat", EventMerger::Kind::Beat, { 0, 10, 20, 30, 40, 50, 60 }, params)
    };

    auto activations = EventMerger::buildActivations (streams, numFrames, params);

    GIVEN("the reference default, which discards its kernel weights")
    {
        REQUIRE_FALSE(params.applyKernelWeights);

        auto summed = EventMerger::summedActivation (activations, params);

        THEN("there is one value per frame")
        {
            REQUIRE(summed.size() == (size_t) numFrames);
        }

        THEN("it is the plain sum of the convolved columns")
        {
            const auto points = std::min (params.maxKernelPoints, numFrames);

            std::vector<float> expected ((size_t) numFrames, 0.0f);

            for (size_t i = 0; i < activations.columns.size(); ++i)
            {
                auto kernel = EventMerger::ricker (points,
                                                   activations.intervals[i] / params.kernelWidthDivisor);
                auto convolved = EventMerger::convolveSame (activations.columns[i], kernel);

                for (size_t frame = 0; frame < expected.size(); ++frame)
                    expected[frame] += convolved[frame];
            }

            for (size_t frame = 0; frame < expected.size(); ++frame)
                REQUIRE(summed[frame] == Catch::Approx(expected[frame]).margin(1e-4f));
        }
    }

    GIVEN("the kernel weights enabled")
    {
        params.applyKernelWeights = true;

        auto weighted = EventMerger::summedActivation (activations, params);

        params.applyKernelWeights = false;
        auto plain = EventMerger::summedActivation (activations, params);

        THEN("column i is scaled by 1 + i, so the two differ")
        {
            REQUIRE(weighted.size() == plain.size());

            auto anyDifference = false;

            for (size_t frame = 0; frame < weighted.size(); ++frame)
                if (std::abs (weighted[frame] - plain[frame]) > 1e-4f)
                    anyDifference = true;

            REQUIRE(anyDifference);
        }
    }

    GIVEN("a stream whose events all land on one frame")
    {
        // Two events quantising to the same frame give a zero interval, and so
        // no usable kernel width.
        const std::vector<EventMerger::EventStream> degenerate {
            streamFromFrames ("flat", EventMerger::Kind::Beat, { 20, 20, 20 }, params),
            streamFromFrames ("seg", EventMerger::Kind::Segmentation, { 0, 60, 120 }, params)
        };

        auto degenerateActivations = EventMerger::buildActivations (degenerate, numFrames, params);

        THEN("it contributes nothing rather than poisoning the sum")
        {
            REQUIRE(degenerateActivations.columns.size() == 2);

            auto summed = EventMerger::summedActivation (degenerateActivations, params);

            REQUIRE(summed.size() == (size_t) numFrames);

            for (auto value : summed)
                REQUIRE(std::isfinite (value));
        }
    }
}

SCENARIO("EventMerger picks the strongest frames deterministically",
         "[engine][analysis][merge]")
{
    EventMerger::Parameters params;
    params.peakIndexOffset = 0;

    GIVEN("a clear ranking")
    {
        const std::vector<float> activation { 1.0f, 9.0f, 3.0f, 7.0f, 2.0f };

        THEN("the strongest frames come back in ascending order")
        {
            REQUIRE(EventMerger::pickPeaks (activation, 2, params) == std::vector<int> { 1, 3 });
            REQUIRE(EventMerger::pickPeaks (activation, 3, params) == std::vector<int> { 1, 2, 3 });
        }
    }

    GIVEN("tied activations")
    {
        const std::vector<float> activation { 5.0f, 5.0f, 5.0f, 5.0f };

        THEN("ties resolve by frame order rather than arbitrarily")
        {
            // np.argpartition leaves this unspecified, which is why parity with
            // the reference is only guaranteed to a frame.
            REQUIRE(EventMerger::pickPeaks (activation, 2, params) == std::vector<int> { 0, 1 });
            REQUIRE(EventMerger::pickPeaks (activation, 2, params)
                        == EventMerger::pickPeaks (activation, 2, params));
        }
    }

    GIVEN("more segments requested than there are frames")
    {
        const std::vector<float> activation { 1.0f, 2.0f, 3.0f };

        THEN("the request is clamped instead of running off the end")
        {
            // The reference raises here.
            REQUIRE(EventMerger::pickPeaks (activation, 10, params) == std::vector<int> { 0, 1, 2 });
        }
    }

    GIVEN("the reference's index offset")
    {
        params.peakIndexOffset = -2;
        const std::vector<float> activation { 1.0f, 2.0f, 9.0f, 8.0f, 3.0f };

        THEN("every picked index is shifted by it")
        {
            REQUIRE(EventMerger::pickPeaks (activation, 2, params) == std::vector<int> { 0, 1 });
        }

        THEN("indices shifted off the front of the grid are kept for the next stage")
        {
            const std::vector<float> earlyPeak { 9.0f, 8.0f, 1.0f };
            REQUIRE(EventMerger::pickPeaks (earlyPeak, 2, params) == std::vector<int> { -2, -1 });
        }
    }

    GIVEN("degenerate arguments")
    {
        THEN("nothing is picked")
        {
            REQUIRE(EventMerger::pickPeaks ({}, 5, params).empty());
            REQUIRE(EventMerger::pickPeaks ({ 1.0f, 2.0f }, 0, params).empty());
        }
    }
}

SCENARIO("EventMerger produces boundaries end to end", "[engine][analysis][merge]")
{
    EventMerger::Parameters params;
    params.numSegments = 5;

    EventMerger merger;

    GIVEN("segment boundaries and a beat grid over a minute of material")
    {
        const auto duration = 60.0f;
        const auto numFrames = EventMerger::frameCount (duration, params);

        std::vector<int> segFrames, beatFrames;

        for (auto frame = 0; frame < numFrames; frame += 400)
            segFrames.push_back (frame);

        for (auto frame = 0; frame < numFrames; frame += 43)
            beatFrames.push_back (frame);

        const std::vector<EventMerger::EventStream> streams {
            streamFromFrames ("sbic", EventMerger::Kind::Segmentation, segFrames, params),
            streamFromFrames ("beats", EventMerger::Kind::Beat, beatFrames, params)
        };

        WHEN("the streams are merged")
        {
            auto result = merger.merge (streams, duration, params);

            THEN("boundaries are produced")
            {
                REQUIRE_FALSE(result.boundaries.empty());
                REQUIRE(result.boundaries.size() <= (size_t) params.numSegments);
            }

            THEN("they are ascending and inside the material")
            {
                for (size_t i = 0; i < result.boundaries.size(); ++i)
                {
                    REQUIRE(result.boundaries[i] >= 0.0f);
                    REQUIRE(result.boundaries[i] <= duration);

                    if (i > 0)
                        REQUIRE(result.boundaries[i] > result.boundaries[i - 1]);
                }
            }

            THEN("the activation it picked them from is returned alongside")
            {
                REQUIRE(result.activation.size() == (size_t) numFrames);

                for (auto value : result.activation)
                    REQUIRE(std::isfinite (value));
            }
        }
    }
}
