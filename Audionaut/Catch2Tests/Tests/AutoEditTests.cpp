#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Factory/AudioTrackFactory.h"
#include "Engine/Analysis/AnalysisProvider.h"
#include "Engine/AutoEdit/AutoEdit.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Group/ResourceGroup.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/Resource/AudioResource.h"
#include "Engine/Resource/AudioResourceContainer.h"

// The native auto edit reads analyses from the cache and never runs one, so
// these scenarios populate the cache directly and need no Essentia.

using namespace audium;

namespace {

/**
 * An engine holding one audio track, plus the file its analyses are keyed to.
 *
 * Nothing inside the engine is cached as a member. UndoableContainerAction
 * serialises the track container and rebuilds it, so every shared_ptr taken
 * before an undoable edit dangles afterwards - the accessors below re-query
 * instead.
 */
struct Fixture {
    std::shared_ptr<AudiumEngine> engine;
    juce::File analysedFile;

    // Adding an audio file already creates a region covering the whole file,
    // so scenarios compare against what was there rather than against zero.
    size_t baselineRegions = 0;

    std::shared_ptr<AudioTrack> track() const
    {
        return engine->getAudioTrackContainer()->getAudioTrack(0);
    }

    std::shared_ptr<ResourceGroup> resourceGroup() const
    {
        auto audioTrack = track();

        if (audioTrack == nullptr || audioTrack->getResourceGroups().empty())
            return nullptr;

        return audioTrack->getResourceGroups()[0];
    }

    std::vector<std::shared_ptr<AudioRegion>> regions() const
    {
        auto group = resourceGroup();

        if (group == nullptr)
            return {};

        return group->getAudioRegionContainer()->getObjects();
    }

    size_t regionCount() const { return regions().size(); }

    double audioLengthSeconds() const
    {
        auto group = resourceGroup();

        if (group == nullptr || group->getAudioResources().empty())
            return 0.0;

        return group->getAudioResources()[0]->getFileLength(audium::seconds);
    }

    ~Fixture()
    {
        engine = nullptr;
    }
};

std::vector<float> evenTimes(float first, float interval, int count)
{
    std::vector<float> times;

    for (auto i = 0; i < count; ++i)
        times.push_back(first + interval * (float) i);

    return times;
}

Fixture makeFixture()
{
    Fixture fixture;
    fixture.engine = AudiumFactory::createAudiumEngine();

    auto testFilesDirectory = String(CURRENT_SOURCE_DIR) + String("/TestFiles/");
    auto inFile = File(testFilesDirectory + "_export_TRK-18.wav");
    REQUIRE(inFile.existsAsFile());

    fixture.engine->getAudioTrackContainer()->addAudioFiles({ inFile.getFullPathName() },
                                                            0.0,
                                                            nullptr,
                                                            false);

    auto group = fixture.resourceGroup();
    REQUIRE(group != nullptr);
    REQUIRE_FALSE(group->getAudioResources().empty());

    // The resource may reference a copy rather than the file handed in, so the
    // cache has to be keyed off whatever it actually points at.
    fixture.analysedFile = File(group->getAudioResources()[0]->getFullPathName());
    fixture.baselineRegions = fixture.regionCount();

    return fixture;
}

// Populates the analyses the merge needs, shaped like real results.
void cacheMergeAnalyses(const Fixture& fixture)
{
    auto cache = fixture.engine->getAudioTrackContainer()->getAnalysisProvider()->getCache();

    cache->put(fixture.analysedFile, AnalysisType::SBic, evenTimes(0.0f, 2.5f, 8));
    cache->put(fixture.analysedFile, AnalysisType::BeatDegara, evenTimes(0.0f, 0.5f, 40));
}

} // namespace

SCENARIO("AutoEdit cuts a track into regions from its cached analyses",
         "[engine][autoedit]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    {
        auto fixture = makeFixture();
        cacheMergeAnalyses(fixture);

        const auto baseline = fixture.baselineRegions;

        AutoEdit autoEdit(fixture.engine);

        AutoEditConfig config;
        config.trackId = 0;
        config.numSegments = 6;
        REQUIRE(config.source == AutoEditConfig::Source::Native);

        std::string reportedError;
        auto onError = [&reportedError](std::string error) { reportedError = error; };

        WHEN("the track is auto edited")
        {
            const auto succeeded = autoEdit.invokeAutoEdit(config, onError);

            THEN("it reports success without an error")
            {
                INFO("error was: " << reportedError);
                REQUIRE(succeeded);
                REQUIRE(reportedError.empty());
            }

            THEN("regions are created on the track's resource group")
            {
                REQUIRE(fixture.regionCount() > baseline);
            }

            THEN("the new regions tile the material without gaps or overlaps")
            {
                auto regions = fixture.regions();
                REQUIRE(regions.size() > baseline);

                const auto length = fixture.audioLengthSeconds();
                REQUIRE(length > 0.0);

                // The whole-file region added with the audio is still there, so
                // only the ones this edit produced are checked for tiling.
                std::vector<juce::Range<double>> created;

                for (const auto& region : regions)
                    if (region->getName().startsWith("seg-"))
                        created.push_back(region->getRegionData(audium::seconds));

                REQUIRE_FALSE(created.empty());

                std::sort(created.begin(), created.end(),
                          [](const auto& a, const auto& b) { return a.getStart() < b.getStart(); });

                double previousEnd = -1.0;

                for (const auto& range : created)
                {
                    REQUIRE(range.getStart() >= 0.0);
                    REQUIRE(range.getEnd() <= length + 0.001);
                    REQUIRE(range.getLength() > 0.0);

                    if (previousEnd >= 0.0)
                        REQUIRE(range.getStart() == Catch::Approx(previousEnd).margin(0.001));

                    previousEnd = range.getEnd();
                }
            }

            THEN("one undo removes every region the edit created")
            {
                auto undoManager = fixture.engine->getAudioTrackContainer()->getUndoManager();
                REQUIRE(undoManager->canUndo());

                undoManager->undo();

                REQUIRE(fixture.regionCount() == baseline);
            }
        }
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("AutoEdit refuses to edit before the analyses are cached",
         "[engine][autoedit]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    {
        auto fixture = makeFixture();

        // Deliberately cache only one of the two the merge needs. A partial
        // merge would produce plausible but different cuts.
        auto cache = fixture.engine->getAudioTrackContainer()->getAnalysisProvider()->getCache();
        cache->put(fixture.analysedFile, AnalysisType::SBic, evenTimes(0.0f, 2.5f, 8));

        AutoEdit autoEdit(fixture.engine);

        AutoEditConfig config;
        config.trackId = 0;

        std::string reportedError;
        auto onError = [&reportedError](std::string error) { reportedError = error; };

        WHEN("the track is auto edited")
        {
            const auto succeeded = autoEdit.invokeAutoEdit(config, onError);

            THEN("it declines rather than cutting on a partial set")
            {
                REQUIRE_FALSE(succeeded);
                REQUIRE(fixture.regionCount() == fixture.baselineRegions);
            }

            THEN("it names the analysis still outstanding")
            {
                REQUIRE_FALSE(reportedError.empty());
                REQUIRE(reportedError.find(analysisTypeToString(AnalysisType::BeatDegara))
                            != std::string::npos);
            }
        }
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("AutoEdit reports an unusable target instead of failing silently",
         "[engine][autoedit]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    {
        auto engine = AudiumFactory::createAudiumEngine();

        AutoEdit autoEdit(engine);

        std::string reportedError;
        auto onError = [&reportedError](std::string error) { reportedError = error; };

        WHEN("no such track exists")
        {
            AutoEditConfig config;
            config.trackId = 99;

            THEN("it declines with a reason")
            {
                REQUIRE_FALSE(autoEdit.invokeAutoEdit(config, onError));
                REQUIRE_FALSE(reportedError.empty());
            }
        }

        engine = nullptr;
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("AutoEdit edits the audio the chosen playlist item refers to",
         "[engine][autoedit]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    {
        auto engine = AudiumFactory::createAudiumEngine();

        auto testFilesDirectory = String(CURRENT_SOURCE_DIR) + String("/TestFiles/");
        auto firstFile = File(testFilesDirectory + "_export_TRK-18.wav");
        auto secondFile = File(testFilesDirectory + "120-funk-export.wav");
        REQUIRE(firstFile.existsAsFile());
        REQUIRE(secondFile.existsAsFile());

        auto trackContainer = engine->getAudioTrackContainer();

        // The container makes a new track per call, so the second file is added
        // to the same track directly - and at a position no playlist item
        // occupies, so it gets its own resource group and item rather than
        // joining the first as another channel.
        trackContainer->addAudioFiles({ firstFile.getFullPathName() }, 0.0, nullptr, false);

        auto track = trackContainer->getAudioTrack(0);
        REQUIRE(track != nullptr);

        track->addAudioFiles({ secondFile.getFullPathName() }, 1.0e9, nullptr, false);

        auto playListItems = track->getPlayListContainer()->getPlayListItems();

        // Only meaningful when the track really does carry two separate items.
        if (playListItems.size() < 2)
            SKIP("the two audio files did not produce two playlist items");

        auto secondGroup = playListItems[1]->getRegion()->getResourceGroup();
        REQUIRE(secondGroup != nullptr);
        REQUIRE(secondGroup != playListItems[0]->getRegion()->getResourceGroup());

        auto secondResource = secondGroup->getAudioResources()[0];
        const auto secondAnalysed = File(secondResource->getFullPathName());

        // Cache analyses for the second item's audio only. If the edit resolved
        // its target any other way it would find nothing cached and decline.
        auto cache = trackContainer->getAnalysisProvider()->getCache();
        cache->put(secondAnalysed, AnalysisType::SBic, evenTimes(0.0f, 0.25f, 5));
        cache->put(secondAnalysed, AnalysisType::BeatDegara, evenTimes(0.0f, 0.1f, 12));

        const auto baseline = secondGroup->getAudioRegionContainer()->getObjects().size();

        AutoEdit autoEdit(engine);

        AutoEditConfig config;
        config.trackId = 0;
        config.playlistItemId = 1;
        config.numSegments = 4;

        std::string reportedError;
        auto onError = [&reportedError](std::string error) { reportedError = error; };

        WHEN("the second playlist item is auto edited")
        {
            const auto succeeded = autoEdit.invokeAutoEdit(config, onError);

            THEN("it edits that item's audio, not the track's first")
            {
                INFO("error was: " << reportedError);
                REQUIRE(succeeded);

                // Re-query: the undoable action rebuilt the container.
                auto items = trackContainer->getAudioTrack(0)->getPlayListContainer()->getPlayListItems();
                REQUIRE(items.size() >= 2);

                auto group = items[1]->getRegion()->getResourceGroup();
                REQUIRE(group->getAudioRegionContainer()->getObjects().size() > baseline);
            }
        }

        engine = nullptr;
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}
