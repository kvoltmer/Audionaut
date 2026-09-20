#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Engine/Core/DspLoadMeter.h"

#include <thread>

using namespace audium;
using Catch::Approx;

SCENARIO("dsp load meter scenario", "[engine][load-meter]")
{
    GIVEN("a fresh DspLoadMeter")
    {
        DspLoadMeter meter;

        THEN("it reports no load, no peak and no overruns")
        {
            REQUIRE(meter.getLoad() == 0.0f);
            REQUIRE(meter.takePeak() == 0.0f);
            REQUIRE(meter.getOverrunCount() == 0);
        }

        WHEN("many callbacks each use half their budget")
        {
            for (int i = 0; i < 100; ++i)
                meter.record(0.5, 1.0);

            THEN("the smoothed load converges on 50%")
            {
                REQUIRE(meter.getLoad() == Approx(0.5f).margin(0.01f));
            }

            THEN("the peak is 50% and is cleared once taken")
            {
                REQUIRE(meter.takePeak() == Approx(0.5f));
                REQUIRE(meter.takePeak() == 0.0f);
            }

            THEN("nothing counts as an overrun")
            {
                REQUIRE(meter.getOverrunCount() == 0);
            }
        }

        WHEN("one callback spikes among quiet ones")
        {
            meter.record(0.1, 1.0);
            meter.record(0.8, 1.0);
            meter.record(0.1, 1.0);

            THEN("the peak remembers the spike while the load stays low")
            {
                REQUIRE(meter.takePeak() == Approx(0.8f));
                REQUIRE(meter.getLoad() < 0.5f);
            }
        }

        WHEN("a callback exceeds its budget")
        {
            meter.record(1.5, 1.0);
            meter.record(1.0, 1.0);

            THEN("each overrun is counted and the peak exceeds 100%")
            {
                REQUIRE(meter.getOverrunCount() == 2);
                REQUIRE(meter.takePeak() == Approx(1.5f));
            }

            AND_WHEN("the meter is reset")
            {
                meter.reset();

                THEN("everything is back to zero")
                {
                    REQUIRE(meter.getLoad() == 0.0f);
                    REQUIRE(meter.takePeak() == 0.0f);
                    REQUIRE(meter.getOverrunCount() == 0);
                }
            }
        }

        WHEN("a zero or negative budget is recorded")
        {
            meter.record(0.5, 0.0);
            meter.record(-1.0, 1.0);

            THEN("the sample is ignored")
            {
                REQUIRE(meter.getLoad() == 0.0f);
                REQUIRE(meter.takePeak() == 0.0f);
            }
        }

        WHEN("timing a block that takes longer than the audio it produces")
        {
            // 64 samples at 48kHz is a 1.33ms budget; sleep well past it
            meter.begin();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            meter.end(64, 48000.0);

            THEN("it registers as an overrun")
            {
                REQUIRE(meter.takePeak() > 1.0f);
                REQUIRE(meter.getOverrunCount() == 1);
            }
        }

        WHEN("timing a trivially short block")
        {
            meter.begin();
            meter.end(4096, 48000.0);

            THEN("the load is far below the budget")
            {
                REQUIRE(meter.takePeak() < 0.5f);
                REQUIRE(meter.getOverrunCount() == 0);
            }
        }
    }
}
