#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Engine/AutoEdit/AutoEditParameter.h"

// Pure arithmetic - no engine, no Essentia. The measure convention (four
// beats to a measure) matches TempoProvider, which these values pin.

using namespace audium;

SCENARIO("AutoEditParameter derives the segment count from measures and tempo",
         "[engine][autoedit][parameter]")
{
    GIVEN("a four-measure parameter and material at 120 bpm")
    {
        const AutoEditParameter parameter(4.0);

        THEN("the parameter is active")
        {
            REQUIRE(parameter.isActive());
        }

        // Four beats at 120 bpm: half a second each.
        THEN("a measure lasts two seconds")
        {
            REQUIRE(AutoEditParameter::measureSeconds(120.0) == Catch::Approx(2.0));
        }

        THEN("two minutes cut into eight-second segments gives fifteen")
        {
            REQUIRE(parameter.numSegmentsFor(120.0, 120.0) == 15);
        }

        THEN("the count rounds to the nearest whole segment")
        {
            // 100 s / 8 s = 12.5 segments.
            REQUIRE(parameter.numSegmentsFor(100.0, 120.0) == 13);
        }

        THEN("material shorter than one segment still yields one")
        {
            REQUIRE(parameter.numSegmentsFor(3.0, 120.0) == 1);
        }
    }

    GIVEN("inputs nothing can be derived from")
    {
        THEN("a zero-measure parameter is off and derives nothing")
        {
            const AutoEditParameter parameter(0.0);

            REQUIRE_FALSE(parameter.isActive());
            REQUIRE(parameter.numSegmentsFor(120.0, 120.0) == 0);
        }

        THEN("an unknown tempo derives nothing")
        {
            REQUIRE(AutoEditParameter::measureSeconds(0.0) == 0.0);
            REQUIRE(AutoEditParameter(4.0).numSegmentsFor(120.0, 0.0) == 0);
        }

        THEN("an empty duration derives nothing")
        {
            REQUIRE(AutoEditParameter(4.0).numSegmentsFor(0.0, 120.0) == 0);
        }
    }
}
