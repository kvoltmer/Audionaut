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

namespace {

// RhythmExtractor2013's "multifeature" method computes several candidate beat
// sequences - one of which comes from the same "degara" method BeatSegmenter
// can also be asked to run on its own - and picks the one with the best
// mutual agreement among them. So a Degara-only sequence is expected to be
// close to, but not necessarily identical to, MultiFeature's output: both
// should agree on roughly the same tempo and land beats within a small
// tolerance of each other for most of the track.
constexpr float toleranceSeconds = 0.15f;

size_t countMatchesWithinTolerance(const std::vector<float>& reference,
                                    const std::vector<float>& candidate)
{
    size_t matches = 0;
    for (auto refBeat : reference)
    {
        for (auto candBeat : candidate)
        {
            if (std::abs(refBeat - candBeat) <= toleranceSeconds)
            {
                ++matches;
                break;
            }
        }
    }
    return matches;
}

} // namespace

SCENARIO("BeatSegmenter's Degara method agrees with its MultiFeature method", "[engine][analysis][beats]")
{
    auto testFilesDirectory = String(CURRENT_SOURCE_DIR) + String("/TestFiles/");
    auto inFile = File(testFilesDirectory + "TRK-18-epy-jul.wav");
    REQUIRE(inFile.existsAsFile());

    GIVEN("the same audio file analysed with both methods")
    {
        BeatSegmenter segmenter;

        BeatSegmenter::Parameters multiFeatureParams;
        multiFeatureParams.method = BeatSegmenter::Method::MultiFeature;

        BeatSegmenter::Parameters degaraParams;
        degaraParams.method = BeatSegmenter::Method::Degara;

        auto multiFeatureBeats = segmenter.analyze(inFile, multiFeatureParams).beats;
        auto degaraBeats = segmenter.analyze(inFile, degaraParams).beats;

        THEN("both return beat timestamps")
        {
            REQUIRE_FALSE(multiFeatureBeats.empty());
            REQUIRE_FALSE(degaraBeats.empty());
        }

        THEN("they estimate a similar number of beats")
        {
            auto larger = std::max(multiFeatureBeats.size(), degaraBeats.size());
            auto smaller = std::min(multiFeatureBeats.size(), degaraBeats.size());

            // The two methods should agree on tempo to within a small margin,
            // so their beat counts shouldn't diverge wildly.
            REQUIRE((float) smaller / (float) larger > 0.7f);
        }

        THEN("most beats reported by one method fall close to a beat reported by the other")
        {
            auto matches = countMatchesWithinTolerance(multiFeatureBeats, degaraBeats);

            REQUIRE((float) matches / (float) multiFeatureBeats.size() > 0.7f);
        }
    }
}

#endif // ESSENTIA_ENABLED
