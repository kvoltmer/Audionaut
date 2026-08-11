#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Analysis/AnalysisProvider.h"
#include "Engine/AutoEdit/AutoEdit.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Group/ResourceGroup.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/Region/AudioRegion.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/Resource/AudioResource.h"
#include "Engine/Resource/AudioResourceContainer.h"

// The assemble step arranges the regions a previous auto edit cut, so the
// engine scenarios run a native auto edit first - fed from a hand-populated
// analysis cache, like the AutoEdit scenarios, so no Essentia is needed.

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

    std::vector<std::shared_ptr<PlayListItem>> playListItems() const
    {
        return track()->getPlayListContainer()->getPlayListItems();
    }

    juce::StringArray regionNames() const
    {
        juce::StringArray names;

        for (const auto& region : regions())
            names.add(region->getName());

        return names;
    }

    // The names of the items' regions in playlist order - the assembled song's
    // sequence. Names identify regions across the container rebuilds undoable
    // edits perform, where pointers do not survive.
    juce::StringArray playListRegionNames() const
    {
        juce::StringArray names;

        for (const auto& item : playListItems())
            names.add(item->getRegion()->getName());

        return names;
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

    return fixture;
}

// Populates the analyses the merge needs, shaped like real results.
void cacheMergeAnalyses(const Fixture& fixture)
{
    auto cache = fixture.engine->getAudioTrackContainer()->getAnalysisProvider()->getCache();

    cache->put(fixture.analysedFile, AnalysisType::SBic, evenTimes(0.0f, 2.5f, 8));
    cache->put(fixture.analysedFile, AnalysisType::BeatDegara, evenTimes(0.0f, 0.5f, 40));
}

// Cuts the fixture's clip into segments, giving the assemble scenarios several
// regions of varying length to arrange.
void cutIntoSegments(Fixture& fixture)
{
    cacheMergeAnalyses(fixture);

    AutoEdit autoEdit(fixture.engine);

    AutoEditConfig config;
    config.trackId = 0;
    config.playlistItemId = 0;
    config.numSegments = 6;

    std::string reportedError;
    REQUIRE(autoEdit.invokeAutoEdit(config, [&reportedError](std::string error)
    {
        reportedError = error;
    }));
    INFO("auto edit error was: " << reportedError);
    REQUIRE(fixture.regions().size() > 1);
}

} // namespace

TEST_CASE("chooseRandomSequence draws segments until the target duration is filled",
          "[assemble]")
{
    const std::vector<double> lengths { 2.0, 3.5, 1.25, 4.0, 0.75 };
    const auto target = 20.0;

    std::mt19937 rng(1234);
    const auto chosen = AutoEdit::chooseRandomSequence(lengths, target, rng);

    SECTION("the same seed reproduces the same sequence")
    {
        std::mt19937 again(1234);
        REQUIRE(AutoEdit::chooseRandomSequence(lengths, target, again) == chosen);
    }

    SECTION("every index points into the input")
    {
        for (auto index : chosen)
        {
            REQUIRE(index >= 0);
            REQUIRE((size_t) index < lengths.size());
        }
    }

    SECTION("the total reaches the target, overshooting by at most one segment")
    {
        double total = 0.0;

        for (auto index : chosen)
            total += lengths[(size_t) index];

        REQUIRE(total >= target);

        const auto lastLength = lengths[(size_t) chosen.back()];
        REQUIRE(total - lastLength < target);
    }
}

TEST_CASE("chooseRandomSequence handles degenerate input", "[assemble]")
{
    std::mt19937 rng(1234);

    SECTION("a single segment is drawn repeatedly until the target is reached")
    {
        const auto chosen = AutoEdit::chooseRandomSequence({ 1.0 }, 5.0, rng);
        REQUIRE(chosen == std::vector<int>(5, 0));
    }

    SECTION("no segments choose nothing")
    {
        REQUIRE(AutoEdit::chooseRandomSequence({}, 60.0, rng).empty());
    }

    SECTION("a non-positive target chooses nothing")
    {
        REQUIRE(AutoEdit::chooseRandomSequence({ 1.0, 2.0 }, 0.0, rng).empty());
        REQUIRE(AutoEdit::chooseRandomSequence({ 1.0, 2.0 }, -1.0, rng).empty());
    }
}

TEST_CASE("chooseSequentialSequence thins the segments but keeps their order",
          "[assemble]")
{
    const std::vector<double> lengths { 2.0, 3.5, 1.25, 4.0, 0.75, 2.5 };

    SECTION("a target matching the total keeps every segment once, in order")
    {
        std::mt19937 rng(1234);
        const auto total = std::accumulate(lengths.begin(), lengths.end(), 0.0);

        const auto chosen = AutoEdit::chooseSequentialSequence(lengths, total, rng);
        REQUIRE(chosen == std::vector<int> { 0, 1, 2, 3, 4, 5 });
    }

    SECTION("a target beyond the total repeats the material in order")
    {
        std::mt19937 rng(1234);
        const auto total = std::accumulate(lengths.begin(), lengths.end(), 0.0);
        const auto target = total * 2.5;

        const auto chosen = AutoEdit::chooseSequentialSequence(lengths, target, rng);

        double sum = 0.0;

        for (size_t i = 0; i < chosen.size(); ++i)
        {
            // Nothing is thinned when the target exceeds the total, so the
            // walk is the plain wrapped order.
            REQUIRE(chosen[i] == (int) (i % lengths.size()));
            sum += lengths[(size_t) chosen[i]];
        }

        REQUIRE(sum >= target);
        REQUIRE(sum - lengths[(size_t) chosen.back()] < target);
    }

    SECTION("the thinned walk reaches the target, overshooting by at most one segment")
    {
        std::mt19937 rng(1234);
        const auto chosen = AutoEdit::chooseSequentialSequence(lengths, 7.0, rng);

        double sum = 0.0;

        for (auto index : chosen)
        {
            REQUIRE(index >= 0);
            REQUIRE((size_t) index < lengths.size());
            sum += lengths[(size_t) index];
        }

        REQUIRE(sum >= 7.0);
        REQUIRE(sum - lengths[(size_t) chosen.back()] < 7.0);
    }

    SECTION("the same seed reproduces the same choice")
    {
        std::mt19937 rng(1234);
        std::mt19937 again(1234);

        REQUIRE(AutoEdit::chooseSequentialSequence(lengths, 7.0, again)
                == AutoEdit::chooseSequentialSequence(lengths, 7.0, rng));
    }

    SECTION("no segments or a non-positive target choose nothing")
    {
        std::mt19937 rng(1234);
        REQUIRE(AutoEdit::chooseSequentialSequence({}, 60.0, rng).empty());
        REQUIRE(AutoEdit::chooseSequentialSequence(lengths, 0.0, rng).empty());
    }
}

SCENARIO("Assemble rebuilds the track's playlist from its regions",
         "[engine][assemble]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    {
        auto fixture = makeFixture();
        cutIntoSegments(fixture);

        const auto regionNamesBefore = fixture.regionNames();

        AutoEdit autoEdit(fixture.engine);

        AssembleConfig config;
        config.trackId = 0;

        std::string reportedError;
        auto onError = [&reportedError](std::string error) { reportedError = error; };

        WHEN("a random song of ten seconds is assembled")
        {
            config.mode = AssembleConfig::Mode::Random;
            config.duration = 10.0;

            const auto succeeded = autoEdit.invokeAssemble(config, onError);

            THEN("it reports success without an error")
            {
                INFO("error was: " << reportedError);
                REQUIRE(succeeded);
                REQUIRE(reportedError.empty());
            }

            THEN("the clips are butt-joined from the start of the timeline")
            {
                auto items = fixture.playListItems();
                REQUIRE_FALSE(items.empty());

                double expectedPosition = 0.0;

                for (const auto& item : items)
                {
                    REQUIRE(item->getAbsolutePosition(audium::clocks)
                            == Catch::Approx(expectedPosition).margin(0.001));
                    expectedPosition += item->getRegionData(audium::clocks).getLength();
                }
            }

            THEN("the song is at least as long as asked for")
            {
                double totalSeconds = 0.0;

                for (const auto& item : fixture.playListItems())
                    totalSeconds += item->getRegion()->getRegionData(audium::seconds).getLength();

                REQUIRE(totalSeconds >= config.duration);
            }

            THEN("every clip plays one of the regions that already existed")
            {
                for (const auto& item : fixture.playListItems())
                    REQUIRE(regionNamesBefore.contains(item->getRegion()->getName()));
            }

            THEN("no region was created or destroyed")
            {
                REQUIRE(fixture.regionNames() == regionNamesBefore);
            }

            THEN("assembling again with the same seed makes the same song")
            {
                const auto firstSong = fixture.playListRegionNames();

                REQUIRE(autoEdit.invokeAssemble(config, onError));
                REQUIRE(fixture.playListRegionNames() == firstSong);
            }
        }

        WHEN("a sequential song of exactly the material's length is assembled")
        {
            // Summed in the same order invokeAssemble sums, so the duration
            // matches the total bit for bit and one full pass fills it.
            juce::StringArray eligibleNames;
            double totalSeconds = 0.0;

            for (const auto& region : fixture.track()->getRegions())
            {
                const auto length = region->getRegionData(audium::seconds).getLength();

                if (length > 0.0)
                {
                    eligibleNames.add(region->getName());
                    totalSeconds += length;
                }
            }

            config.mode = AssembleConfig::Mode::Sequential;
            config.duration = totalSeconds;

            const auto succeeded = autoEdit.invokeAssemble(config, onError);

            THEN("every region appears exactly once, in region order")
            {
                INFO("error was: " << reportedError);
                REQUIRE(succeeded);
                REQUIRE(fixture.playListRegionNames() == eligibleNames);
            }
        }

        WHEN("a sequential song longer than the material is assembled")
        {
            config.mode = AssembleConfig::Mode::Sequential;
            config.duration = 100.0;

            const auto succeeded = autoEdit.invokeAssemble(config, onError);

            THEN("the material repeats until the song is long enough")
            {
                INFO("error was: " << reportedError);
                REQUIRE(succeeded);

                double totalSeconds = 0.0;

                for (const auto& item : fixture.playListItems())
                    totalSeconds += item->getRegion()->getRegionData(audium::seconds).getLength();

                REQUIRE(totalSeconds >= config.duration);
            }
        }

        WHEN("the previous arrangement is assembled over")
        {
            const auto itemsBefore = fixture.playListRegionNames();

            config.mode = AssembleConfig::Mode::Random;
            config.duration = 10.0;

            REQUIRE(autoEdit.invokeAssemble(config, onError));

            THEN("one undo restores it")
            {
                REQUIRE(fixture.playListRegionNames() != itemsBefore);

                auto undoManager = fixture.engine->getAudioTrackContainer()->getUndoManager();
                REQUIRE(undoManager->canUndo());

                undoManager->undo();

                REQUIRE(fixture.playListRegionNames() == itemsBefore);
            }
        }
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("Assemble reports why nothing could be assembled and leaves the "
         "arrangement alone",
         "[engine][assemble]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    {
        auto fixture = makeFixture();
        cutIntoSegments(fixture);

        const auto itemsBefore = fixture.playListRegionNames();

        AutoEdit autoEdit(fixture.engine);

        std::string reportedError;
        auto onError = [&reportedError](std::string error) { reportedError = error; };

        WHEN("no valid track is named")
        {
            AssembleConfig config;
            REQUIRE(config.trackId == -1);

            THEN("it fails with a message and the arrangement is untouched")
            {
                REQUIRE_FALSE(autoEdit.invokeAssemble(config, onError));
                REQUIRE_FALSE(reportedError.empty());
                REQUIRE(fixture.playListRegionNames() == itemsBefore);
            }
        }

        WHEN("the duration is zero")
        {
            AssembleConfig config;
            config.trackId = 0;
            config.duration = 0.0;

            THEN("it fails with a message and the arrangement is untouched")
            {
                REQUIRE_FALSE(autoEdit.invokeAssemble(config, onError));
                REQUIRE_FALSE(reportedError.empty());
                REQUIRE(fixture.playListRegionNames() == itemsBefore);
            }
        }

        WHEN("a sequential draw keeps nothing")
        {
            // A vanishingly small target makes the keep probability so small
            // that the walk exhausts its visit cap without keeping anything -
            // the assemble must then leave the arrangement as it found it
            // rather than empty it.
            AssembleConfig config;
            config.trackId = 0;
            config.mode = AssembleConfig::Mode::Sequential;
            config.duration = 1.0e-9;

            THEN("it fails with a message and the arrangement is untouched")
            {
                REQUIRE_FALSE(autoEdit.invokeAssemble(config, onError));
                REQUIRE_FALSE(reportedError.empty());
                REQUIRE(fixture.playListRegionNames() == itemsBefore);
            }
        }
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}
