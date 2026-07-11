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

    WHEN("results of several types are stored for the same and different files")
    {
        auto other = File(testFilesDirectory + "another.wav");
        cache.put(file, AnalysisType::SBic, { 1.0f, 2.0f });
        cache.put(file, AnalysisType::Onset, { 3.0f });
        cache.put(other, AnalysisType::SBic, { 4.0f });

        THEN("getAll returns only the per-file results for the requested type")
        {
            auto sbic = cache.getAll(AnalysisType::SBic);
            REQUIRE(sbic.size() == 2);
            REQUIRE(sbic.at(file.getFullPathName().toStdString()) == std::vector<float>{ 1.0f, 2.0f });
            REQUIRE(sbic.at(other.getFullPathName().toStdString()) == std::vector<float>{ 4.0f });

            auto onset = cache.getAll(AnalysisType::Onset);
            REQUIRE(onset.size() == 1);

            REQUIRE(cache.getAll(AnalysisType::Beat).empty());
        }
    }
}

SCENARIO("AnalysisCache persists to and loads from a project folder",
         "[engine][analysis][cache][serialization]")
{
    auto testFilesDirectory = String(CURRENT_SOURCE_DIR) + String("/TestFiles/");
    auto sourceFile = File(testFilesDirectory + "_export_TRK-18.wav");
    REQUIRE(sourceFile.existsAsFile());

    // Work in a throwaway directory that stands in for the .audium package.
    auto projectFolder = File::getSpecialLocation(File::tempDirectory)
                             .getChildFile("AudionautAnalysisCacheTest_"
                                           + String(Time::currentTimeMillis()));
    REQUIRE(projectFolder.createDirectory().wasOk());

    // Analyse a private copy so we can safely mutate its modification time.
    auto audioFile = projectFolder.getChildFile("audio.wav");
    REQUIRE(sourceFile.copyFileTo(audioFile));

    const std::vector<float> sbic { 0.0f, 1.5f, 3.25f };
    const std::vector<float> onset { 0.5f, 2.0f };

    GIVEN("a populated cache")
    {
        AnalysisCache cache;
        cache.put(audioFile, AnalysisType::SBic, sbic);
        cache.put(audioFile, AnalysisType::Onset, onset);

        WHEN("it is saved to the project folder")
        {
            REQUIRE(cache.saveToFolder(projectFolder));

            THEN("an AnalysisData.json sidecar is written")
            {
                REQUIRE(projectFolder.getChildFile("AnalysisData.json").existsAsFile());
            }

            THEN("a fresh cache loads the same results back")
            {
                AnalysisCache loaded;
                REQUIRE(loaded.loadFromFolder(projectFolder));
                REQUIRE(loaded.size() == 2);
                REQUIRE(loaded.get(audioFile, AnalysisType::SBic) == sbic);
                REQUIRE(loaded.get(audioFile, AnalysisType::Onset) == onset);
            }

            THEN("loading discards any pre-existing entries first")
            {
                AnalysisCache loaded;
                loaded.put(File(testFilesDirectory + "stale.wav"), AnalysisType::Beat, { 9.0f });
                REQUIRE(loaded.loadFromFolder(projectFolder));
                REQUIRE(loaded.size() == 2);
                REQUIRE(loaded.getAll(AnalysisType::Beat).empty());
            }

            THEN("results are treated as stale once the file is modified")
            {
                audioFile.setLastModificationTime(Time::getCurrentTime() + RelativeTime::hours(1));

                AnalysisCache loaded;
                REQUIRE(loaded.loadFromFolder(projectFolder));
                // The entry is still present, but keyed to the old identity, so
                // a lookup against the now-changed file misses.
                REQUIRE_FALSE(loaded.get(audioFile, AnalysisType::SBic).has_value());
            }
        }
    }

    GIVEN("a project saved with analysis data, then moved on disk")
    {
        // Analyse and save inside 'orig', with the audio under that folder.
        auto origFolder = projectFolder.getChildFile("orig");
        REQUIRE(origFolder.createDirectory().wasOk());
        auto origAudio = origFolder.getChildFile("Media/audio.wav");
        REQUIRE(origAudio.getParentDirectory().createDirectory().wasOk());
        REQUIRE(sourceFile.copyFileTo(origAudio));

        AnalysisCache cache;
        cache.put(origAudio, AnalysisType::SBic, sbic);
        REQUIRE(cache.saveToFolder(origFolder));

        WHEN("the whole project folder is renamed/moved")
        {
            auto movedFolder = projectFolder.getChildFile("moved");
            REQUIRE(origFolder.moveFileTo(movedFolder));

            auto movedAudio = movedFolder.getChildFile("Media/audio.wav");
            REQUIRE(movedAudio.existsAsFile());

            THEN("loading from the new location still resolves the results")
            {
                AnalysisCache loaded;
                REQUIRE(loaded.loadFromFolder(movedFolder));
                REQUIRE(loaded.get(movedAudio, AnalysisType::SBic) == sbic);
            }
        }
    }

    GIVEN("analysis done on audio that is then relocated (Save-As)")
    {
        // Analyse the audio at its original (e.g. temporary) location.
        auto oldFolder = projectFolder.getChildFile("temp/Media/Audio");
        REQUIRE(oldFolder.createDirectory().wasOk());
        auto oldAudio = oldFolder.getChildFile("audio.wav");
        REQUIRE(sourceFile.copyFileTo(oldAudio));

        AnalysisCache cache;
        cache.put(oldAudio, AnalysisType::SBic, sbic);

        WHEN("the audio is copied into the project and the cache is rebased")
        {
            auto newFolder = projectFolder.getChildFile("saved/Media/Audio");
            REQUIRE(newFolder.createDirectory().wasOk());
            auto newAudio = newFolder.getChildFile("audio.wav");
            REQUIRE(oldAudio.copyFileTo(newAudio));

            cache.rebaseAudioFolder(newFolder);

            THEN("results are found under the new path and not the old one")
            {
                REQUIRE(cache.get(newAudio, AnalysisType::SBic) == sbic);
                REQUIRE_FALSE(cache.get(oldAudio, AnalysisType::SBic).has_value());
                REQUIRE(cache.size() == 1);
            }

            THEN("saving then loading resolves against the new project folder")
            {
                auto savedProject = projectFolder.getChildFile("saved");
                REQUIRE(cache.saveToFolder(savedProject));

                AnalysisCache loaded;
                REQUIRE(loaded.loadFromFolder(savedProject));
                REQUIRE(loaded.get(newAudio, AnalysisType::SBic) == sbic);
            }
        }

        WHEN("rebased against a folder that has no matching file")
        {
            auto emptyFolder = projectFolder.getChildFile("nowhere");
            REQUIRE(emptyFolder.createDirectory().wasOk());

            cache.rebaseAudioFolder(emptyFolder);

            THEN("the entry is left pointing at its original file")
            {
                REQUIRE(cache.get(oldAudio, AnalysisType::SBic) == sbic);
            }
        }
    }

    GIVEN("an empty cache")
    {
        AnalysisCache cache;

        THEN("saving writes no sidecar file")
        {
            REQUIRE(cache.saveToFolder(projectFolder));
            REQUIRE_FALSE(projectFolder.getChildFile("AnalysisData.json").existsAsFile());
        }
    }

    GIVEN("a project folder without a sidecar")
    {
        auto emptyFolder = projectFolder.getChildFile("empty");
        REQUIRE(emptyFolder.createDirectory().wasOk());

        THEN("loading is a clean no-op")
        {
            AnalysisCache cache;
            cache.put(File(testFilesDirectory + "x.wav"), AnalysisType::SBic, { 1.0f });
            REQUIRE(cache.loadFromFolder(emptyFolder));
            REQUIRE(cache.size() == 0);
        }
    }

    projectFolder.deleteRecursively();
}
