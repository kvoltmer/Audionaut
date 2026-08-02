#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Engine/Analysis/AnalysisProvider.h"

// makeMergeStreams() and the cache lookups are free of Essentia, so most of
// these run unconditionally. Only the end-to-end scenario needs a real analysis.
#ifndef ESSENTIA_ENABLED
 #if __has_include(<essentia/algorithmfactory.h>) && __has_include(<unsupported/Eigen/CXX11/Tensor>)
  #define ESSENTIA_ENABLED 1
 #else
  #define ESSENTIA_ENABLED 0
 #endif
#endif

using namespace audium;

namespace {

std::shared_ptr<AnalysisProvider> makeProvider(std::shared_ptr<AnalysisCache> cache)
{
    // The segmenters do no work until analyze() is called, so constructing them
    // here costs nothing and needs no Essentia.
    return std::make_shared<AnalysisProvider>(std::make_shared<SBicSegmenter>(),
                                              std::make_shared<OnsetSegmenter>(),
                                              std::make_shared<BeatSegmenter>(),
                                              cache);
}

// Evenly spaced event times, in seconds.
std::vector<float> evenTimes(float first, float interval, int count)
{
    std::vector<float> times;

    for (auto i = 0; i < count; ++i)
        times.push_back(first + interval * (float) i);

    return times;
}

File testFile()
{
    return File(String(CURRENT_SOURCE_DIR) + String("/TestFiles/_export_TRK-18.wav"));
}

} // namespace

SCENARIO("AnalysisProvider assembles the merge's input streams",
         "[engine][analysis][merge]")
{
    GIVEN("results from both analyses")
    {
        auto streams = AnalysisProvider::makeMergeStreams(evenTimes(0.0f, 10.0f, 6),
                                                          evenTimes(0.0f, 0.5f, 100));

        THEN("each analysis contributes one stream")
        {
            REQUIRE(streams.size() == 2);
        }

        THEN("the structural boundaries lead, as the merge expects")
        {
            REQUIRE(streams[0].label == "sbic");
            REQUIRE(streams[0].kind == EventMerger::Kind::Segmentation);
        }

        THEN("beats are an event stream, not a structural one")
        {
            REQUIRE(streams[1].label == "beats_degara");
            REQUIRE(streams[1].kind == EventMerger::Kind::Beat);
        }

        THEN("the event times are carried through unchanged")
        {
            REQUIRE(streams[0].times.size() == 6);
            REQUIRE(streams[0].times[0] == Catch::Approx(0.0f));
            REQUIRE(streams[0].times[5] == Catch::Approx(50.0f));
            REQUIRE(streams[1].times.size() == 100);
        }
    }

    GIVEN("one analysis that found nothing")
    {
        auto streams = AnalysisProvider::makeMergeStreams(evenTimes(0.0f, 10.0f, 6), {});

        THEN("it contributes no stream at all, rather than an empty one")
        {
            REQUIRE(streams.size() == 1);
            REQUIRE(streams[0].label == "sbic");
        }
    }

    GIVEN("no results at all")
    {
        auto streams = AnalysisProvider::makeMergeStreams({}, {});

        THEN("there is nothing to merge")
        {
            REQUIRE(streams.empty());
        }
    }
}

SCENARIO("AnalysisProvider merges only what the cache already holds",
         "[engine][analysis][merge]")
{
    auto cache = std::make_shared<AnalysisCache>();
    auto provider = makeProvider(cache);

    const auto audioFile = testFile();
    REQUIRE(audioFile.existsAsFile());

    const auto duration = 60.0f;

    GIVEN("an unanalysed file")
    {
        THEN("both analyses are reported missing")
        {
            auto missing = provider->findMissingMergeAnalyses(audioFile);
            REQUIRE(missing.size() == 2);
        }

        THEN("merging yields nothing rather than running an analysis")
        {
            auto result = provider->mergeCachedAnalyses(audioFile, duration);
            REQUIRE(result.boundaries.empty());
            REQUIRE(result.activation.empty());
        }
    }

    GIVEN("only some of the analyses cached")
    {
        cache->put(audioFile, AnalysisType::SBic, evenTimes(0.0f, 10.0f, 6));

        THEN("the outstanding one is named")
        {
            auto missing = provider->findMissingMergeAnalyses(audioFile);
            REQUIRE(missing.size() == 1);
            REQUIRE(missing[0] == AnalysisType::BeatDegara);
        }

        THEN("merging still yields nothing - a partial merge would mislead")
        {
            auto result = provider->mergeCachedAnalyses(audioFile, duration);
            REQUIRE(result.boundaries.empty());
        }
    }

    GIVEN("both analyses cached")
    {
        cache->put(audioFile, AnalysisType::SBic, evenTimes(0.0f, 7.5f, 8));
        cache->put(audioFile, AnalysisType::BeatDegara, evenTimes(0.0f, 0.5f, 120));

        THEN("nothing is reported missing")
        {
            REQUIRE(provider->findMissingMergeAnalyses(audioFile).empty());
        }

        WHEN("they are merged")
        {
            EventMerger::Parameters params;
            params.numSegments = 8;

            auto result = provider->mergeCachedAnalyses(audioFile, duration, params);

            THEN("boundaries come back")
            {
                REQUIRE_FALSE(result.boundaries.empty());
            }

            THEN("they are ascending and inside the material")
            {
                for (size_t i = 0; i < result.boundaries.size(); ++i)
                {
                    REQUIRE(result.boundaries[i] >= 0.0f);
                    REQUIRE(result.boundaries[i] <= duration);

                    if (i > 0)
                        REQUIRE(result.boundaries[i] > result.boundaries[i - 1]);
                }
            }

            THEN("the run opens at the start of the material")
            {
                REQUIRE(result.boundaries.front() == Catch::Approx(0.0f));
            }
        }

        THEN("the merge does not write itself back to the cache")
        {
            const auto before = cache->size();
            provider->mergeCachedAnalyses(audioFile, duration);
            REQUIRE(cache->size() == before);
        }
    }
}

#if ESSENTIA_ENABLED
SCENARIO("AnalysisProvider merges analyses it produced itself",
         "[engine][analysis][merge][essentia]")
{
    auto cache = std::make_shared<AnalysisCache>();
    auto provider = makeProvider(cache);

    const auto audioFile = testFile();
    REQUIRE(audioFile.existsAsFile());

    GIVEN("the file analysed with each of the merge's analyses")
    {
        for (auto analysisType : AnalysisProvider::getMergeAnalysisTypes())
            provider->analyzeFile(audioFile, analysisType);

        REQUIRE(provider->findMissingMergeAnalyses(audioFile).empty());

        WHEN("the cached results are merged")
        {
            // Comfortably longer than the test file; the merge only needs an
            // upper bound on the material.
            const auto duration = 120.0f;
            auto result = provider->mergeCachedAnalyses(audioFile, duration);

            THEN("it produces ascending boundaries inside the material")
            {
                REQUIRE_FALSE(result.boundaries.empty());

                for (size_t i = 0; i < result.boundaries.size(); ++i)
                {
                    REQUIRE(result.boundaries[i] >= 0.0f);
                    REQUIRE(result.boundaries[i] <= duration);

                    if (i > 0)
                        REQUIRE(result.boundaries[i] > result.boundaries[i - 1]);
                }
            }
        }
    }
}
#endif // ESSENTIA_ENABLED
