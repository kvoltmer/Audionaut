#include <catch2/catch_test_macros.hpp>

#include "Engine/Analysis/BeatSegmenter.h"

// This scenario needs real analysis results, which are only available when the
// codebase is built against Essentia. See ESSENTIA_ENABLED in the segmenters.
#ifndef ESSENTIA_ENABLED
 #if __has_include(<essentia/algorithmfactory.h>) && __has_include(<unsupported/Eigen/CXX11/Tensor>)
  #define ESSENTIA_ENABLED 1
 #else
  #define ESSENTIA_ENABLED 0
 #endif
#endif

using namespace audium;

#if ESSENTIA_ENABLED
SCENARIO("BeatSegmenter tracks beats in an audio file", "[engine][analysis][beats]")
{
    auto testFilesDirectory = String(CURRENT_SOURCE_DIR) + String("/TestFiles/");
    auto inFile = File(testFilesDirectory + "_export_TRK-18.wav");
    REQUIRE(inFile.existsAsFile());

    BeatSegmenter segmenter;

    WHEN("the file is analysed with default parameters")
    {
        auto beats = segmenter.analyze(inFile);

        THEN("it returns beat timestamps")
        {
            REQUIRE_FALSE(beats.empty());
        }

        THEN("the timestamps are non-negative and strictly increasing (in seconds)")
        {
            for (size_t i = 0; i < beats.size(); ++i)
            {
                REQUIRE(beats[i] >= 0.0f);
                if (i > 0)
                    REQUIRE(beats[i] > beats[i - 1]);
            }
        }
    }
}
#endif // ESSENTIA_ENABLED

SCENARIO("BeatSegmenter returns no beats for a missing file", "[engine][analysis][beats]")
{
    BeatSegmenter segmenter;
    auto missing = File("/non/existent/audio/file.wav");
    REQUIRE_FALSE(missing.existsAsFile());

    auto beats = segmenter.analyze(missing);
    REQUIRE(beats.empty());
}
