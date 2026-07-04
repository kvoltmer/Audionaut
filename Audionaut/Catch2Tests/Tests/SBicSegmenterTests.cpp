#include <catch2/catch_test_macros.hpp>

#include "Engine/Analysis/SBicSegmenter.h"

using namespace audium;

SCENARIO("SBicSegmenter segments an audio file", "[engine][analysis][segmentation]")
{
    auto testFilesDirectory = String(CURRENT_SOURCE_DIR) + String("/TestFiles/");
    auto inFile = File(testFilesDirectory + "_export_TRK-18.wav");
    REQUIRE(inFile.existsAsFile());

    SBicSegmenter segmenter;

    WHEN("the file is analysed with default parameters")
    {
        auto segments = segmenter.analyze(inFile);

        THEN("it returns segment boundaries")
        {
            REQUIRE_FALSE(segments.empty());
        }

        THEN("the timestamps are non-negative and strictly increasing (in seconds)")
        {
            for (size_t i = 0; i < segments.size(); ++i)
            {
                REQUIRE(segments[i] >= 0.0f);
                if (i > 0)
                    REQUIRE(segments[i] > segments[i - 1]);
            }
        }
    }
}

SCENARIO("SBicSegmenter returns no segments for a missing file", "[engine][analysis][segmentation]")
{
    SBicSegmenter segmenter;
    auto missing = File("/non/existent/audio/file.wav");
    REQUIRE_FALSE(missing.existsAsFile());

    auto segments = segmenter.analyze(missing);
    REQUIRE(segments.empty());
}
