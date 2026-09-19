#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Project/ProjectSerializer.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/Selection/SelectionManager.h"

using namespace audium;

static const auto selectionTestFilesDirectory = String(CURRENT_SOURCE_DIR) + String("/TestFiles/");

SCENARIO("pasting clipboard JSON with stale ids is skipped instead of crashing", "[engine][selection][paste]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());
    auto engine = AudiumFactory::createAudiumEngine();
    auto tracks = engine->getAudioTrackContainer();
    auto selection = tracks->getSelectionManager();

    auto audioFile = File(selectionTestFilesDirectory + "120-funk-1-sec.wav");
    REQUIRE(audioFile.existsAsFile());

    GIVEN("a project with one clip") {
        engine->getProjectSerializer()->createNewProject();
        REQUIRE(tracks->addAudioFiles({ audioFile.getFullPathName() }, 0.0, nullptr, false));

        std::shared_ptr<AudioTrack> clipTrack;
        for (auto i = 0; i < tracks->getNumItems(); ++i)
            if (tracks->getAudioTrack(i)->getPlayListContainer()->playListItems.size() > 0)
                clipTrack = tracks->getAudioTrack(i);
        REQUIRE(clipTrack != nullptr);
        REQUIRE(clipTrack->getPlayListContainer()->playListItems.size() == 1);

        auto totalClips = [&] {
            size_t n = 0;
            for (auto i = 0; i < tracks->getNumItems(); ++i)
                n += tracks->getAudioTrack(i)->getPlayListContainer()->playListItems.size();
            return n;
        };
        auto totalRegions = [&] {
            size_t n = 0;
            for (auto i = 0; i < tracks->getNumItems(); ++i)
                for (auto& group : tracks->getAudioTrack(i)->getResourceGroups())
                    n += group->getAudioRegionContainer()->getObjects().size();
            return n;
        };

        nlohmann::json clipJson;
        REQUIRE(clipTrack->getPlayListContainer()->playListItems.getObjects()[0]->writeToJson(clipJson));

        WHEN("a copied clip is pasted onto its own track") {
            nlohmann::json data;
            data["lola"]["play_list_items"] = nlohmann::json::array({ clipJson });
            const auto before = totalClips();
            selection->pasteFromJson(data, engine, false);

            THEN("one clip is added") {
                REQUIRE(totalClips() == before + 1);
            }
        }

        WHEN("the clip's source track no longer exists") {
            auto stale = clipJson;
            stale["track_id"] = tracks->getNumItems() + 5;
            nlohmann::json data;
            data["lola"]["play_list_items"] = nlohmann::json::array({ stale });
            const auto before = totalClips();

            THEN("the paste is skipped without touching the arrangement") {
                REQUIRE_NOTHROW(selection->pasteFromJson(data, engine, false));
                REQUIRE(totalClips() == before);
            }
        }

        WHEN("the clipboard carries an empty clip list") {
            nlohmann::json data;
            data["lola"]["play_list_items"] = nlohmann::json::array();

            THEN("nothing happens") {
                REQUIRE_NOTHROW(selection->pasteFromJson(data, engine, false));
            }
        }

        WHEN("a copied region points at a resource group that is gone") {
            nlohmann::json region;
            region["track_id"] = clipTrack->getId();
            region["resource_group_id"] = 42;
            region["name"] = "stale";
            nlohmann::json data;
            data["lola"]["audio_regions"] = nlohmann::json::array({ region });
            const auto before = totalRegions();

            THEN("the region is skipped without touching the project") {
                REQUIRE_NOTHROW(selection->pasteFromJson(data, engine, false));
                REQUIRE(totalRegions() == before);
            }
        }
    }

    engine = nullptr;
    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}
