#include <catch2/catch_test_macros.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Factory/AudioTrackFactory.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/Region/AudioRegion.h"

#include "TestUtils.h"

using namespace audium;

// std::shared_ptr<PlayListItem> PlayListContainer::createPlayListItemUI(std::shared_ptr<AudioRegion> region, int insertIndex)


SCENARIO("PlayList", "[engine][playlist]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());
    auto engine     = AudiumFactory::createAudiumEngine();
    auto store = engine->getProjectFileStore();
    
    auto testFilesDirectory = String("../../../TestFiles/");
    auto inFile = File(testFilesDirectory + "silence-fade.aiff");
    
    
    GIVEN("new project") {
        
        store->open(inFile, nullptr);
        
        WHEN("") {
            
            // TODO: testing .... argh....


            THEN("") {

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
