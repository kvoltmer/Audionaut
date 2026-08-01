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
