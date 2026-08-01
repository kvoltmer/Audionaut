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
