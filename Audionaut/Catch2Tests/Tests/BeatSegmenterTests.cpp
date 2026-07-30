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
SCENARIO("BeatSegmenter tracks beats in an audio file", "[engine][essentia][analysis][beats]")
{
    auto testFilesDirectory = String(CURRENT_SOURCE_DIR) + String("/TestFiles/");
    auto inFile = File(testFilesDirectory + "TRK-18-epy-jul.wav");
    REQUIRE(inFile.existsAsFile());

    BeatSegmenter segmenter;

    WHEN("the file is analysed with default parameters")
    {
        auto result = segmenter.analyze(inFile);
        const auto& beats = result.beats;

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

        THEN("it returns a positive BPM estimate")
        {
            REQUIRE(result.bpm > 0.0f);
        }
    }

    WHEN("the file is analysed with the Degara method")
    {
        BeatSegmenter::Parameters params;
        params.method = BeatSegmenter::Method::Degara;

        auto result = segmenter.analyze(inFile, params);
        const auto& beats = result.beats;

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

        THEN("it returns a positive BPM estimate")
        {
            REQUIRE(result.bpm > 0.0f);
        }
    }
}
#endif // ESSENTIA_ENABLED

SCENARIO("BeatSegmenter returns no beats for a missing file", "[engine][essentia][analysis][beats]")
{
    BeatSegmenter segmenter;
    auto missing = File("/non/existent/audio/file.wav");
    REQUIRE_FALSE(missing.existsAsFile());

    auto result = segmenter.analyze(missing);
    REQUIRE(result.beats.empty());
    REQUIRE(result.bpm == 0.0f);
}
