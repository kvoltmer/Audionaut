#include <algorithm>
#include <cmath>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Factory/AudioTrackFactory.h"
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

    // The names of those pre-existing regions. Segments are named after the
    // track through the container's unique-name mechanism, so an edit's
    // regions are recognised as the ones that were not there before.
    juce::StringArray baselineNames;

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

    bool isCreated(const std::shared_ptr<AudioRegion>& region) const
    {
        return ! baselineNames.contains(region->getName());
    }

    // The regions an edit created: everything not present at fixture creation.
    std::vector<std::shared_ptr<AudioRegion>> createdRegions() const
    {
        std::vector<std::shared_ptr<AudioRegion>> result;

        for (const auto& region : regions())
            if (isCreated(region))
                result.push_back(region);

        return result;
    }

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

    for (const auto& region : fixture.regions())
        fixture.baselineNames.add(region->getName());

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

                for (const auto& region : fixture.createdRegions())
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
        auto secondFile = File(testFilesDirectory + "TRK-18-epy-jul.wav");
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
        // Shaped like cacheMergeAnalyses() so the merge places cuts clear of
        // the clip's edges, where the sub-beat-sliver rule would skip them.
        auto cache = trackContainer->getAnalysisProvider()->getCache();
        cache->put(secondAnalysed, AnalysisType::SBic, evenTimes(0.0f, 2.5f, 8));
        cache->put(secondAnalysed, AnalysisType::BeatDegara, evenTimes(0.0f, 0.5f, 40));

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

SCENARIO("AutoEdit cuts only what the selected clip covers", "[engine][autoedit]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    {
        auto fixture = makeFixture();
        cacheMergeAnalyses(fixture);

        // Shrink the clip to the middle of the file. The analyses still
        // describe all of it, so anything outside this window has to be
        // discarded rather than cut.
        const juce::Range<double> clip { 4.0, 12.0 };

        {
            auto items = fixture.track()->getPlayListContainer()->getPlayListItems();
            REQUIRE_FALSE(items.empty());
            items[0]->getRegion()->setRegionData(clip, audium::seconds);
        }

        const auto baseline = fixture.regionCount();

        AutoEdit autoEdit(fixture.engine);

        AutoEditConfig config;
        config.trackId = 0;
        config.playlistItemId = 0;
        config.numSegments = 8;

        std::string reportedError;
        auto onError = [&reportedError](std::string error) { reportedError = error; };

        WHEN("the clip is auto edited")
        {
            const auto succeeded = autoEdit.invokeAutoEdit(config, onError);

            THEN("regions are created")
            {
                INFO("error was: " << reportedError);
                REQUIRE(succeeded);
                REQUIRE(fixture.regionCount() > baseline);
            }

            THEN("none of them reaches outside the clip")
            {
                std::vector<juce::Range<double>> created;

                for (const auto& region : fixture.createdRegions())
                    created.push_back(region->getRegionData(audium::seconds));

                REQUIRE_FALSE(created.empty());

                for (const auto& range : created)
                {
                    REQUIRE(range.getStart() >= clip.getStart() - 0.001);
                    REQUIRE(range.getEnd() <= clip.getEnd() + 0.001);
                }
            }

            THEN("together they span the clip exactly")
            {
                std::vector<juce::Range<double>> created;

                for (const auto& region : fixture.createdRegions())
                    created.push_back(region->getRegionData(audium::seconds));

                std::sort(created.begin(), created.end(),
                          [](const auto& a, const auto& b) { return a.getStart() < b.getStart(); });

                REQUIRE(created.front().getStart() == Catch::Approx(clip.getStart()).margin(0.001));
                REQUIRE(created.back().getEnd() == Catch::Approx(clip.getEnd()).margin(0.001));
            }
        }
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("AutoEdit's first segment begins where the clip does", "[engine][autoedit]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    {
        auto fixture = makeFixture();
        cacheMergeAnalyses(fixture);

        // A clip that starts partway into its audio: the edit covers what the
        // clip covers, so the first segment starts where it does and the last
        // ends where it ends.
        const juce::Range<double> clip { 3.0, 15.0 };

        {
            auto items = fixture.track()->getPlayListContainer()->getPlayListItems();
            REQUIRE_FALSE(items.empty());
            items[0]->getRegion()->setRegionData(clip, audium::seconds);
        }

        AutoEdit autoEdit(fixture.engine);

        AutoEditConfig config;
        config.trackId = 0;
        config.numSegments = 8;
        // -1 is what the dialog sends when its clip list is empty, which it is
        // for clips under a second. The clip's start still has to be honoured.
        config.playlistItemId = GENERATE(0, -1);

        std::string reportedError;

        WHEN("the clip is auto edited")
        {
            const auto succeeded = autoEdit.invokeAutoEdit(
                config, [&reportedError](std::string e) { reportedError = e; });

            INFO("error was: " << reportedError);
            REQUIRE(succeeded);

            std::vector<juce::Range<double>> created;

            for (const auto& region : fixture.createdRegions())
                created.push_back(region->getRegionData(audium::seconds));

            std::sort(created.begin(), created.end(),
                      [](const auto& a, const auto& b) { return a.getStart() < b.getStart(); });

            REQUIRE_FALSE(created.empty());

            THEN("the first segment starts at the clip's start")
            {
                REQUIRE(created.front().getStart() == Catch::Approx(3.0).margin(0.001));
            }

            THEN("the last segment ends at the clip's end")
            {
                REQUIRE(created.back().getEnd() == Catch::Approx(15.0).margin(0.001));
            }

            THEN("they tile the clip without gaps")
            {
                for (size_t i = 1; i < created.size(); ++i)
                    REQUIRE(created[i].getStart()
                                == Catch::Approx(created[i - 1].getEnd()).margin(0.001));
            }
        }
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("AutoEdit can replace the edited clip with its segments",
         "[engine][autoedit]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    {
        auto fixture = makeFixture();
        cacheMergeAnalyses(fixture);

        const auto playListItems = [&fixture]
        {
            return fixture.track()->getPlayListContainer()->getPlayListItems();
        };

        REQUIRE(playListItems().size() == 1);
        const auto originalRegionName = playListItems()[0]->getRegion()->getName();

        AutoEdit autoEdit(fixture.engine);

        AutoEditConfig config;
        config.trackId = 0;
        config.playlistItemId = 0;
        config.numSegments = 6;

        std::string reportedError;
        auto onError = [&reportedError](std::string error) { reportedError = error; };

        GIVEN("the option left on, as it is by default")
        {
            REQUIRE(config.replacePlayListItem);

            WHEN("the clip is auto edited")
            {
                INFO("error was: " << reportedError);
                REQUIRE(autoEdit.invokeAutoEdit(config, onError));

                THEN("the arrangement now holds the segments instead of the clip")
                {
                    auto items = playListItems();
                    REQUIRE(items.size() > 1);

                    for (const auto& item : items)
                        REQUIRE(fixture.isCreated(item->getRegion()));
                }

                THEN("the segments are named <track>-seg-<number>")
                {
                    const auto prefix = fixture.track()->getAudioTrackName() + "-seg-";

                    for (const auto& item : playListItems())
                    {
                        const auto name = item->getRegion()->getName();

                        INFO("segment name: " << name);
                        REQUIRE(name.startsWith(prefix));
                        // The "-" separator reads as a sign to JUCE, hence abs.
                        REQUIRE(std::abs(name.getTrailingIntValue()) > 0);
                    }
                }

                THEN("they are in playing order")
                {
                    auto items = playListItems();
                    double previousStart = -1.0;

                    for (const auto& item : items)
                    {
                        const auto start = item->getRegion()->getRegionData(audium::seconds).getStart();
                        REQUIRE(start > previousStart);
                        previousStart = start;
                    }
                }

                THEN("one undo puts the clip back")
                {
                    auto undoManager = fixture.engine->getAudioTrackContainer()->getUndoManager();
                    REQUIRE(undoManager->canUndo());

                    undoManager->undo();

                    auto items = playListItems();
                    REQUIRE(items.size() == 1);
                    REQUIRE(items[0]->getRegion()->getName() == originalRegionName);
                }
            }
        }

        GIVEN("the option turned off")
        {
            config.replacePlayListItem = false;

            WHEN("the clip is auto edited")
            {
                INFO("error was: " << reportedError);
                REQUIRE(autoEdit.invokeAutoEdit(config, onError));

                THEN("the arrangement is left as it was")
                {
                    auto items = playListItems();
                    REQUIRE(items.size() == 1);
                    REQUIRE(items[0]->getRegion()->getName() == originalRegionName);
                }

                THEN("the segments were still created")
                {
                    REQUIRE(fixture.regionCount() > 1);
                }
            }
        }
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("AutoEdit resolves the segment count the edit would cut inside the clip",
         "[engine][autoedit][parameter]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    {
        auto fixture = makeFixture();

        auto cache = fixture.engine->getAudioTrackContainer()->getAnalysisProvider()->getCache();

        AutoEdit autoEdit(fixture.engine);

        AutoEditConfig config;
        config.trackId = 0;
        config.numSegments = 6;

        std::string reportedError;
        auto onError = [&reportedError](std::string error) { reportedError = error; };

        auto createdSegments = [&fixture]() {
            return (int) fixture.createdRegions().size();
        };

        WHEN("the measure parameter is on and the material's tempo is cached")
        {
            // Analyses spanning the whole (roughly one minute) file, with
            // structural events far enough apart to clear the parameter's
            // minimum segment length, so the merge places cuts well inside
            // the clip - the grid snapping the edit applies skips cuts within
            // a beat of the clip's edges.
            cache->put(fixture.analysedFile, AnalysisType::SBic, evenTimes(0.0f, 10.0f, 6));
            cache->put(fixture.analysedFile, AnalysisType::BeatDegara,
                       evenTimes(0.0f, 0.5f, 120), 120.0f);

            config.segmentMeasures = 4.0;

            const auto predicted = autoEdit.resolveNumSegments(config);

            THEN("it predicts exactly the segments the edit then creates")
            {
                REQUIRE(predicted > 0);

                const auto succeeded = autoEdit.invokeAutoEdit(config, onError);

                INFO("error was: " << reportedError);
                REQUIRE(succeeded);
                REQUIRE(createdSegments() == predicted);
            }

            // Four measures at 120 bpm are eight seconds a segment, and the
            // bounds bracket that: half below, double above.
            THEN("the length bounds resolve to four and sixteen seconds")
            {
                const auto bounds = autoEdit.resolveSegmentLengthBounds(config);

                REQUIRE(bounds.getStart() == Catch::Approx(4.0));
                REQUIRE(bounds.getEnd() == Catch::Approx(16.0));
            }
        }

        WHEN("the clip covers only part of the file")
        {
            cacheMergeAnalyses(fixture);

            // The analyses still describe the whole file, but only the
            // boundaries inside this window become cuts - and only they are
            // counted.
            const juce::Range<double> clip { 4.0, 12.0 };

            {
                auto items = fixture.track()->getPlayListContainer()->getPlayListItems();
                REQUIRE_FALSE(items.empty());
                items[0]->getRegion()->setRegionData(clip, audium::seconds);
            }

            config.playlistItemId = 0;
            config.numSegments = 8;

            const auto wholeFile = [&] {
                AutoEditConfig wholeConfig = config;
                wholeConfig.playlistItemId = -1;

                // The fallback target is the same clip, so widen it back for
                // the whole-file reading.
                auto items = fixture.track()->getPlayListContainer()->getPlayListItems();
                const auto restore = items[0]->getRegion()->getRegionData(audium::seconds);
                items[0]->getRegion()->setRegionData({ 0.0, fixture.audioLengthSeconds() },
                                                     audium::seconds);

                const auto count = autoEdit.resolveNumSegments(wholeConfig);
                items[0]->getRegion()->setRegionData(restore, audium::seconds);

                return count;
            }();

            const auto predicted = autoEdit.resolveNumSegments(config);

            THEN("the count is the clip's, not the whole file's")
            {
                REQUIRE(predicted > 0);
                REQUIRE(predicted <= wholeFile);

                const auto succeeded = autoEdit.invokeAutoEdit(config, onError);

                INFO("error was: " << reportedError);
                REQUIRE(succeeded);
                REQUIRE(createdSegments() == predicted);
            }
        }

        WHEN("the analyses are not cached yet")
        {
            config.segmentMeasures = 4.0;

            THEN("there is nothing to count and numSegments is used as given")
            {
                REQUIRE(autoEdit.resolveNumSegments(config) == 6);
            }

            THEN("no length bounds resolve either")
            {
                REQUIRE(autoEdit.resolveSegmentLengthBounds(config).isEmpty());
            }
        }

        WHEN("the parameter is off")
        {
            cacheMergeAnalyses(fixture);

            REQUIRE(config.segmentMeasures == 0.0);

            THEN("no length bounds resolve")
            {
                REQUIRE(autoEdit.resolveSegmentLengthBounds(config).isEmpty());
            }
        }
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("AutoEdit snaps boundaries onto the project's beat grid",
         "[engine][autoedit][gridmatch]")
{
    // 120 BPM: a beat lasts 0.5 s and spans 24 clocks.
    constexpr auto projectTempo = 120.0;

    GIVEN("a clip at the timeline origin playing its file from the start")
    {
        const juce::Range<double> playedRegion(0.0, 10.0);

        THEN("boundaries move to the nearest grid beat")
        {
            const auto snapped = AutoEdit::snapBoundariesToGrid({ 1.02f, 2.98f },
                                                                projectTempo, 0.0, playedRegion);

            REQUIRE(snapped.size() == 2);
            REQUIRE(snapped[0] == Catch::Approx(1.0));
            REQUIRE(snapped[1] == Catch::Approx(3.0));
        }

        THEN("boundaries snapping onto the same beat collapse into one")
        {
            const auto snapped = AutoEdit::snapBoundariesToGrid({ 0.99f, 1.01f },
                                                                projectTempo, 0.0, playedRegion);

            REQUIRE(snapped.size() == 1);
            REQUIRE(snapped[0] == Catch::Approx(1.0));
        }

        THEN("a boundary cutting a sub-beat sliver at the clip's end is skipped")
        {
            // 9.9 s snaps to beat 20 - the clip's end itself - so the cut
            // would leave nothing but a sliver.
            REQUIRE(AutoEdit::snapBoundariesToGrid({ 9.9f },
                                                   projectTempo, 0.0, playedRegion).empty());
        }

        THEN("a boundary cutting a sub-beat sliver at the clip's start is skipped")
        {
            // 0.2 s snaps to beat 0 - the clip's start itself - which would
            // cut nothing but a sliver.
            REQUIRE(AutoEdit::snapBoundariesToGrid({ 0.2f },
                                                   projectTempo, 0.0, playedRegion).empty());
        }

        THEN("a cut exactly one beat from the clip's edge is kept")
        {
            const auto snapped = AutoEdit::snapBoundariesToGrid({ 9.4f },
                                                                projectTempo, 0.0, playedRegion);

            REQUIRE(snapped.size() == 1);
            REQUIRE(snapped[0] == Catch::Approx(9.5));
        }

        THEN("boundaries on the clip's edges pass through unsnapped")
        {
            // An edge boundary makes no cut; clamping it inwards would invent
            // one the analysis never produced.
            const auto snapped = AutoEdit::snapBoundariesToGrid({ 0.0f, 10.0f },
                                                                projectTempo, 0.0, playedRegion);

            REQUIRE(snapped.size() == 2);
            REQUIRE(snapped[0] == Catch::Approx(0.0));
            REQUIRE(snapped[1] == Catch::Approx(10.0));
        }
    }

    GIVEN("a clip sitting off the grid")
    {
        THEN("boundaries snap to the grid, not to the clip's own phase")
        {
            // Clip start 6 clocks = a quarter beat: a boundary one second in
            // sits at 2.25 timeline beats and snaps back to beat 2, which is
            // 0.875 s into the played audio.
            const auto snapped = AutoEdit::snapBoundariesToGrid({ 1.0f },
                                                                projectTempo, 6.0,
                                                                juce::Range<double>(0.0, 10.0));

            REQUIRE(snapped.size() == 1);
            REQUIRE(snapped[0] == Catch::Approx(0.875));
        }
    }

    GIVEN("a clip playing a trimmed part of its file")
    {
        THEN("boundaries stay in file time, offset by the region start")
        {
            // Region starts 0.25 s into the file, clip on the grid: a boundary
            // at 1.27 s file time is 1.02 s into the clip, snaps to timeline
            // beat 2 (1.0 s), and maps back to 1.25 s file time.
            const auto snapped = AutoEdit::snapBoundariesToGrid({ 1.27f },
                                                                projectTempo, 0.0,
                                                                juce::Range<double>(0.25, 10.0));

            REQUIRE(snapped.size() == 1);
            REQUIRE(snapped[0] == Catch::Approx(1.25));
        }
    }

    GIVEN("a clip shorter than a beat")
    {
        THEN("an inside boundary is dropped rather than snapped somewhere it cannot go")
        {
            const auto snapped = AutoEdit::snapBoundariesToGrid({ 0.6f },
                                                                projectTempo, 12.0,
                                                                juce::Range<double>(0.5, 0.7));

            REQUIRE(snapped.empty());
        }
    }
}

SCENARIO("AutoEdit previews only the boundaries the edit would cut",
         "[engine][autoedit][preview]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    {
        auto fixture = makeFixture();
        cacheMergeAnalyses(fixture);

        AutoEdit autoEdit(fixture.engine);

        AutoEditConfig config;
        config.trackId = 0;
        config.numSegments = 6;

        WHEN("a preview is published")
        {
            REQUIRE(autoEdit.previewAutoEdit(config));

            auto analysisProvider = fixture.engine->getAudioTrackContainer()->getAnalysisProvider();
            auto item = fixture.track()->getPlayListContainer()->getPlayListItem(0);
            REQUIRE(item != nullptr);

            const auto preview = analysisProvider->getMergePreview(fixture.analysedFile,
                                                                   0, item->getId());

            // The merge always emits the file's own edges (a boundary at 0 in
            // particular), which the edit never cuts - shown in the preview
            // they would read as an edit right at the clip's start.
            THEN("no previewed boundary sits on or outside the clip's edges")
            {
                REQUIRE_FALSE(preview.empty());

                const auto extent = item->getRegionData(audium::seconds);

                for (auto boundary : preview)
                {
                    REQUIRE(boundary > extent.getStart());
                    REQUIRE(boundary < extent.getEnd());
                }
            }
        }
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("AutoEdit skips edge slivers for clips that do not sit on the grid",
         "[engine][autoedit][gridmatch]")
{
    // 120 BPM: a beat lasts 0.5 s. The clip plays 0..10 s of its file.
    constexpr auto projectTempo = 120.0;
    const juce::Range<double> playedRegion(0.0, 10.0);

    GIVEN("boundaries within a beat of the clip's edges")
    {
        THEN("they are dropped, without moving the surviving cuts")
        {
            const auto kept = AutoEdit::skipEdgeBoundaries({ 0.4f, 3.14f, 9.7f },
                                                           projectTempo, playedRegion);

            REQUIRE(kept.size() == 1);
            REQUIRE(kept[0] == Catch::Approx(3.14));
        }

        THEN("a boundary exactly a beat from an edge is kept")
        {
            const auto kept = AutoEdit::skipEdgeBoundaries({ 0.5f, 9.5f },
                                                           projectTempo, playedRegion);

            REQUIRE(kept.size() == 2);
        }

        THEN("boundaries on or outside the edges pass through for the later filter")
        {
            const auto kept = AutoEdit::skipEdgeBoundaries({ 0.0f, 10.0f },
                                                           projectTempo, playedRegion);

            REQUIRE(kept.size() == 2);
        }
    }
}

SCENARIO("AutoEdit keeps the edited clip's timeline position",
         "[engine][autoedit]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    {
        auto fixture = makeFixture();
        cacheMergeAnalyses(fixture);

        // Move the clip away from the timeline origin while it stays the
        // track's first item - the case where an insert-at-the-beginning
        // heuristic used to shift the first segment left of where the clip
        // sat.
        const auto clipStartClocks = 960.0;

        fixture.track()->getPlayListContainer()->getPlayListItem(0)
            ->setAbsolutePosition(clipStartClocks, audium::clocks);

        AutoEdit autoEdit(fixture.engine);

        AutoEditConfig config;
        config.trackId = 0;
        config.numSegments = 6;

        std::string reportedError;
        auto onError = [&reportedError](std::string error) { reportedError = error; };

        WHEN("the clip is auto edited in place")
        {
            const auto succeeded = autoEdit.invokeAutoEdit(config, onError);

            INFO("error was: " << reportedError);
            REQUIRE(succeeded);

            THEN("the segments tile the timeline from exactly the clip's position")
            {
                // Re-query: the undoable action rebuilt the container.
                auto items = fixture.track()->getPlayListContainer()->getPlayListItems();

                std::vector<std::shared_ptr<PlayListItem>> segments;

                for (const auto& item : items)
                    if (fixture.isCreated(item->getRegion()))
                        segments.push_back(item);

                REQUIRE_FALSE(segments.empty());

                std::sort(segments.begin(), segments.end(), [](const auto& a, const auto& b) {
                    return a->getAbsolutePosition(audium::clocks) < b->getAbsolutePosition(audium::clocks);
                });

                auto expected = clipStartClocks;

                for (const auto& segment : segments)
                {
                    REQUIRE(segment->getAbsolutePosition(audium::clocks)
                                == Catch::Approx(expected));
                    expected += segment->getRegionData(audium::clocks).getLength();
                }
            }
        }
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

