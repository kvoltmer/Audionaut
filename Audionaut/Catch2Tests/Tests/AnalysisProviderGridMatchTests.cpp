#include <catch2/catch_test_macros.hpp>

#include "Engine/Analysis/AnalysisProvider.h"
#include "Engine/Provider/TempoProvider.h"

// matchesGrid() is pure and free of Essentia, so everything here runs
// unconditionally.

using namespace audium;

namespace {

// Evenly spaced beat times, in seconds.
std::vector<float> evenBeats(float first, float interval, int count)
{
    std::vector<float> beats;

    for (auto i = 0; i < count; ++i)
        beats.push_back(first + interval * (float) i);

    return beats;
}

// At 120 BPM a beat lasts 0.5 s and spans 24 clocks.
constexpr double projectTempo = 120.0;
constexpr float beatSeconds = 0.5f;

} // namespace

SCENARIO("AnalysisProvider decides whether beats match the project grid",
         "[engine][analysis][gridmatch]")
{
    GIVEN("a clip at the timeline origin whose beats sit exactly on the grid")
    {
        const auto beats = evenBeats(0.0f, beatSeconds, 16);
        const juce::Range<double> playedRegion(0.0, 8.0);

        THEN("it matches when the detected tempo equals the project tempo")
        {
            REQUIRE(AnalysisProvider::matchesGrid(projectTempo, 120.0f, beats,
                                                  0.0, playedRegion));
        }

        THEN("it still matches with the detected tempo just inside the tolerance")
        {
            const auto insideBpm = (float) (projectTempo
                * (1.0 + AnalysisProvider::tempoMatchTolerance) - 1.0);

            REQUIRE(AnalysisProvider::matchesGrid(projectTempo, insideBpm, beats,
                                                  0.0, playedRegion));
        }

        THEN("it does not match with the detected tempo outside the tolerance")
        {
            const auto outsideBpm = (float) (projectTempo
                * (1.0 + AnalysisProvider::tempoMatchTolerance) + 1.0);

            REQUIRE_FALSE(AnalysisProvider::matchesGrid(projectTempo, outsideBpm, beats,
                                                        0.0, playedRegion));
        }

        THEN("it still matches when the clip starts on a later grid beat")
        {
            REQUIRE(AnalysisProvider::matchesGrid(projectTempo, 120.0f, beats,
                                                  TempoProvider::beatsToClocks(4.0),
                                                  playedRegion));
        }

        THEN("it does not match when the clip starts between grid beats")
        {
            REQUIRE_FALSE(AnalysisProvider::matchesGrid(projectTempo, 120.0f, beats,
                                                        TempoProvider::beatsToClocks(0.5),
                                                        playedRegion));
        }
    }

    GIVEN("beats sitting half a beat off the grid")
    {
        const auto beats = evenBeats(beatSeconds / 2.0f, beatSeconds, 16);
        const juce::Range<double> playedRegion(0.0, 8.0);

        THEN("it does not match despite the matching tempo")
        {
            REQUIRE_FALSE(AnalysisProvider::matchesGrid(projectTempo, 120.0f, beats,
                                                        0.0, playedRegion));
        }
    }

    GIVEN("a clip playing a trimmed part of the file")
    {
        // Beats land off the file's own grid but exactly on the region start,
        // so the played part is grid-aligned once mapped onto the timeline.
        const auto beats = evenBeats(0.25f, beatSeconds, 16);
        const juce::Range<double> playedRegion(0.25, 8.0);

        THEN("beats are measured relative to the played region's start")
        {
            REQUIRE(AnalysisProvider::matchesGrid(projectTempo, 120.0f, beats,
                                                  0.0, playedRegion));
        }
    }

    GIVEN("slightly jittery beat estimates")
    {
        // +-0.02 s is 0.04 beats at 120 BPM - within the mean-deviation
        // tolerance.
        std::vector<float> beats;
        for (auto i = 0; i < 16; ++i)
            beats.push_back((float) i * beatSeconds + ((i % 2 == 0) ? 0.02f : -0.02f));

        const juce::Range<double> playedRegion(0.0, 8.0);

        THEN("it still matches")
        {
            REQUIRE(AnalysisProvider::matchesGrid(projectTempo, 120.0f, beats,
                                                  0.0, playedRegion));
        }
    }

    GIVEN("no beats inside the played region")
    {
        const auto beats = evenBeats(10.0f, beatSeconds, 4);
        const juce::Range<double> playedRegion(0.0, 8.0);

        THEN("it does not match")
        {
            REQUIRE_FALSE(AnalysisProvider::matchesGrid(projectTempo, 120.0f, beats,
                                                        0.0, playedRegion));
        }
    }

    GIVEN("degenerate tempi")
    {
        const auto beats = evenBeats(0.0f, beatSeconds, 16);
        const juce::Range<double> playedRegion(0.0, 8.0);

        THEN("a missing detected BPM never matches")
        {
            REQUIRE_FALSE(AnalysisProvider::matchesGrid(projectTempo, 0.0f, beats,
                                                        0.0, playedRegion));
        }

        THEN("an invalid project tempo never matches")
        {
            REQUIRE_FALSE(AnalysisProvider::matchesGrid(0.0, 120.0f, beats,
                                                        0.0, playedRegion));
        }
    }
}
