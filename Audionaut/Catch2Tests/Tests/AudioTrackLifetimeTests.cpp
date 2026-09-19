#include <catch2/catch_test_macros.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Project/ProjectSerializer.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/PlayList/PlayListContainer.h"

using namespace audium;

static const auto lifetimeTestFilesDirectory = String(CURRENT_SOURCE_DIR) + String("/TestFiles/");

SCENARIO("a track reference may outlive the engine that owned it", "[engine][track][lifetime]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());
    auto engine = AudiumFactory::createAudiumEngine();

    auto audioFile = File(lifetimeTestFilesDirectory + "120-funk-1-sec.wav");
    REQUIRE(audioFile.existsAsFile());

    GIVEN("a project with an empty track and a clip track") {
        engine->getProjectSerializer()->createNewProject();
        auto tracks = engine->getAudioTrackContainer();
        REQUIRE(tracks->addAudioFiles({ audioFile.getFullPathName() }, 0.0, nullptr, false));
        REQUIRE(tracks->getNumItems() >= 2);

        std::vector<std::shared_ptr<AudioTrack>> held;
        for (auto i = 0; i < tracks->getNumItems(); ++i)
            held.push_back(tracks->getAudioTrack(i));
        tracks = nullptr;

        WHEN("the engine is destroyed while the tracks are still referenced") {
            engine = nullptr;

            THEN("the tracks have been cleaned up and can be released afterwards") {
                for (auto& track : held) {
                    REQUIRE(track->getResourceGroups().empty());
                    REQUIRE(track->getPlayListContainer()->playListItems.size() == 0);
                }
                // the destructor must not reach into the (gone) resource container
                held.clear();
                REQUIRE(true);
            }
        }
    }

    engine = nullptr;
    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}
