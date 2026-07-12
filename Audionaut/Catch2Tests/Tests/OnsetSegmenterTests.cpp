#include <catch2/catch_test_macros.hpp>

#include "Engine/Analysis/OnsetSegmenter.h"

// This scenario needs real analysis results, which are only available when the
// codebase is built against Essentia. See ESSENTIA_ENABLED in the segmenters.
#ifndef ESSENTIA_ENABLED
 #define ESSENTIA_ENABLED 1
#endif

using namespace audium;

#if ESSENTIA_ENABLED
SCENARIO("OnsetSegmenter detects onsets in an audio file", "[engine][analysis][onsets]")
{
    auto testFilesDirectory = String(CURRENT_SOURCE_DIR) + String("/TestFiles/");
    auto inFile = File(testFilesDirectory + "_export_TRK-18.wav");
    REQUIRE(inFile.existsAsFile());

    OnsetSegmenter segmenter;

    WHEN("the file is analysed with default parameters")
    {
        auto onsets = segmenter.analyze(inFile);

        THEN("it returns onset timestamps")
        {
            REQUIRE_FALSE(onsets.empty());
        }

        THEN("the timestamps are non-negative and strictly increasing (in seconds)")
        {
            for (size_t i = 0; i < onsets.size(); ++i)
            {
                REQUIRE(onsets[i] >= 0.0f);
                if (i > 0)
                    REQUIRE(onsets[i] > onsets[i - 1]);
            }
        }
    }
}
#endif // ESSENTIA_ENABLED

SCENARIO("OnsetSegmenter returns no onsets for a missing file", "[engine][analysis][onsets]")
{
    OnsetSegmenter segmenter;
    auto missing = File("/non/existent/audio/file.wav");
    REQUIRE_FALSE(missing.existsAsFile());

    auto onsets = segmenter.analyze(missing);
    REQUIRE(onsets.empty());
}
