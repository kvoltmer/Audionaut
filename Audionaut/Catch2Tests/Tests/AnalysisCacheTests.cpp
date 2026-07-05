#include <catch2/catch_test_macros.hpp>

#include "Engine/Analysis/AnalysisCache.h"

using namespace audium;

SCENARIO("AnalysisCache stores and retrieves results", "[engine][analysis][cache]")
{
    auto testFilesDirectory = String(CURRENT_SOURCE_DIR) + String("/TestFiles/");
    auto file = File(testFilesDirectory + "_export_TRK-18.wav");
    REQUIRE(file.existsAsFile());

    AnalysisCache cache;
    const auto analysisType = AnalysisType::SBic;

    GIVEN("an empty cache")
    {
        THEN("a lookup misses")
        {
            REQUIRE_FALSE(cache.get(file, analysisType).has_value());
            REQUIRE(cache.size() == 0);
        }

        WHEN("a result is stored")
        {
            const std::vector<float> result{ 0.0f, 1.5f, 3.25f };
            cache.put(file, analysisType, result);

            THEN("it can be retrieved for the same file and type")
            {
                auto cached = cache.get(file, analysisType);
                REQUIRE(cached.has_value());
                REQUIRE(*cached == result);
                REQUIRE(cache.size() == 1);
            }

            THEN("a different analysis type misses")
            {
                REQUIRE_FALSE(cache.get(file, AnalysisType::Onset).has_value());
            }

            THEN("a different file misses")
            {
                auto other = File(testFilesDirectory + "does-not-exist.wav");
                REQUIRE_FALSE(cache.get(other, analysisType).has_value());
            }

            THEN("clearing removes the entry")
            {
                cache.clear();
                REQUIRE(cache.size() == 0);
                REQUIRE_FALSE(cache.get(file, analysisType).has_value());
            }
        }
    }

    WHEN("a result is stored twice for the same key")
    {
        cache.put(file, analysisType, { 1.0f });
        cache.put(file, analysisType, { 2.0f, 3.0f });

        THEN("the latest value replaces the previous one without adding an entry")
        {
            auto cached = cache.get(file, analysisType);
            REQUIRE(cached.has_value());
            REQUIRE(*cached == std::vector<float>{ 2.0f, 3.0f });
            REQUIRE(cache.size() == 1);
        }
    }
}
