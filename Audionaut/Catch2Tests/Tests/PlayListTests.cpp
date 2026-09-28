#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Engine/Project/ProjectSerializer.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Factory/AudioTrackFactory.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Group/ResourceGroup.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/Region/AudioRegion.h"
#include "Engine/Region/AudioRegionContainer.h"

#include "TestUtils.h"

using namespace audium;

// The UI entry points of the play list: dropping a region onto the list
// (createPlayListItemUI), placing it at a timeline position
// (createPlayListItemAtPositionUI) and deleting a clip with or without its
// region.
SCENARIO("play list items are created from regions and keep their order", "[engine][playlist]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());
    auto engine = AudiumFactory::createAudiumEngine();

    auto inFile = File(String(CURRENT_SOURCE_DIR) + String("/TestFiles/120-funk-1-sec.wav"));
    REQUIRE(inFile.existsAsFile());

    GIVEN("a new project whose track holds one clip at the start") {
        // a fresh project imports into the temp directory rather than into
        // whatever project directory an earlier test left behind
        engine->getProjectSerializer()->createNewProject();
        auto track = engine->getAudioTrackContainer()->getAudioTrack(0);
        REQUIRE(track != nullptr);
        REQUIRE(track->addAudioFiles({ inFile.getFullPathName() }, 0.0, nullptr, false));

        auto playList = track->getPlayListContainer();
        REQUIRE(playList->getNumItems() == 1);
        auto first = playList->getPlayListItem(0);
        auto region = first->getRegion();
        const auto length = first->getDurationTime(clocks);
        REQUIRE(length > 0.0);
        REQUIRE(first->getAbsolutePosition(clocks) == Catch::Approx(0.0).margin(1.0e-9));

        WHEN("a second item is created from the same region at the end of the list") {
            auto second = playList->createPlayListItemUI(region, playList->getNumItems());

            THEN("it follows the first clip back to back") {
                REQUIRE(second != nullptr);
                REQUIRE(playList->getNumItems() == 2);
                REQUIRE(playList->getPlayListItem(1) == second);
                REQUIRE(second->getRegion() == region);
                REQUIRE(first->getAbsolutePosition(clocks) == Catch::Approx(0.0).margin(1.0e-9));
                REQUIRE(second->getAbsolutePosition(clocks) == Catch::Approx(length));
                REQUIRE(playList->sortedByPosition());
                REQUIRE(playList->getTotalLength(clocks) == Catch::Approx(2.0 * length));
            }
        }

        WHEN("an item is inserted before the first clip") {
            auto inserted = playList->createPlayListItemUI(region, 0);

            THEN("it takes the start and pushes the existing clip out of its way") {
                REQUIRE(inserted != nullptr);
                REQUIRE(playList->getNumItems() == 2);
                REQUIRE(playList->getPlayListItem(0) == inserted);
                REQUIRE(inserted->getAbsolutePosition(clocks) == Catch::Approx(0.0).margin(1.0e-9));
                REQUIRE(first->getAbsolutePosition(clocks) == Catch::Approx(length));
                REQUIRE(playList->sortedByPosition());
            }
        }

        WHEN("an item is placed at a free position further down the timeline") {
            const auto position = 3.0 * length;
            auto placed = playList->createPlayListItemAtPositionUI(region, position, clocks);

            THEN("it sits at that position after the first clip, leaving the gap empty") {
                REQUIRE(placed != nullptr);
                REQUIRE(playList->getNumItems() == 2);
                REQUIRE(playList->getPlayListItem(1) == placed);
                REQUIRE(placed->getAbsolutePosition(clocks) == Catch::Approx(position));
                REQUIRE(first->getAbsolutePosition(clocks) == Catch::Approx(0.0).margin(1.0e-9));
                REQUIRE(playList->itemAtAbsolutePosition(position + 0.5 * length, clocks) == placed.get());
                REQUIRE(playList->itemAtAbsolutePosition(2.0 * length, clocks) == nullptr);
                REQUIRE(playList->getTotalLength(clocks) == Catch::Approx(position + length));
            }
        }

        WHEN("the clip is deleted but its region is kept") {
            playList->deletePlayListItem(0);

            THEN("the play list is empty and the region is still available") {
                REQUIRE(playList->getNumItems() == 0);
                REQUIRE_FALSE(playList->exitsInPlayList(region.get()));
                auto regions = region->getResourceGroup()->getAudioRegionContainer();
                REQUIRE(regions->getNumRegions() == 1);
                REQUIRE(regions->getRegion(0) == region);
            }
        }

        WHEN("the clip is deleted together with its region") {
            auto regions = region->getResourceGroup()->getAudioRegionContainer();
            // deleting the region already takes its clips with it, so the
            // return value (the final deleteObject finds nothing) says false
            // here although everything was removed - assert the effect
            playList->deletePlayListItem(first.get(), true);

            THEN("neither the clip nor the region is left") {
                REQUIRE(playList->getNumItems() == 0);
                REQUIRE(regions->getNumRegions() == 0);
            }
        }
    }

    engine = nullptr;
    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}





// A play list item whose region_id points at no region (hand-edited project,
// or left behind by an older bug) used to be kept with a null region and
// crashed later - in deinit(), the scheduler, export. It must be dropped on
// load while the rest of the project still loads.
SCENARIO("play list items with a dangling region_id are dropped on load", "[engine][playlist][json]")
{
    GIVEN("a project with one clip on one track")
    {
        MessageManager::getInstance();

        auto engine = AudiumFactory::createAudiumEngine();
        const auto audioFile = createSlowSawTwoSecondsAudioFile();
        REQUIRE(audioFile.existsAsFile());
        REQUIRE(engine->getProjectFileStore()->open(audioFile, nullptr));

        auto container = engine->getAudioTrackContainer();
        auto playList = container->getAudioTrack(0)->getPlayListContainer();
        REQUIRE(playList->getPlayListItems().size() == 1);
        auto item = playList->getPlayListItem(0);
        REQUIRE_FALSE(item->getVoiceSources().empty());

        json project;
        REQUIRE(container->writeToJson(project));
        auto& items = project["audio_tracks"][0]["play_list_vector"];
        REQUIRE(items.size() == 1);

        const auto requireOnlyValidItems = [&] (size_t expectedCount)
        {
            auto loaded = container->getAudioTrack(0)->getPlayListContainer();
            REQUIRE(loaded->getPlayListItems().size() == expectedCount);
            for (const auto& loadedItem : loaded->getPlayListItems())
                REQUIRE(loadedItem->getRegion() != nullptr);

            // the scheduler walks every item's region - must not trip
            engine->getPlayListScheduler()->commitPlayListData();
        };

        WHEN("a second item references a region that doesn't exist")
        {
            auto dangling = items[0];
            dangling["region_id"] = 999;
            dangling["position_clocks"] = dangling.value("position_clocks", 0.0) + 1.0e6;
            items.push_back(dangling);

            THEN("reloading into the existing graph keeps only the valid clip")
            {
                REQUIRE(container->readFromJson(project, false));
                requireOnlyValidItems(1);
                REQUIRE(container->getAudioTrack(0)->getPlayListContainer()->getPlayListItem(0) == item);
            }

            THEN("rebuilding the graph from it keeps only the valid clip")
            {
                REQUIRE(container->readFromJson(project, true));
                requireOnlyValidItems(1);
            }
        }

        WHEN("the existing item is re-read with a region that doesn't exist")
        {
            items[0]["region_id"] = 999;

            THEN("the item reports failure and releases its voice sources")
            {
                REQUIRE_FALSE(item->readFromJson(items[0], false));
                REQUIRE(item->getRegion() == nullptr);
                REQUIRE(item->getVoiceSources().empty());
            }

            THEN("the play list drops it")
            {
                REQUIRE(container->readFromJson(project, false));
                requireOnlyValidItems(0);
                REQUIRE(item->getVoiceSources().empty());
            }
        }

        item = nullptr;
        playList = nullptr;
        container = nullptr;
        engine = nullptr;
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}
