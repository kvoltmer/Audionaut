#include <catch2/catch_test_macros.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Project/ProjectSerializer.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/AudioSources/VoiceSourceContainer.h"
#include "Engine/AudioSources/VoiceSource.h"

using namespace audium;

static const auto voiceTestFilesDirectory = String(CURRENT_SOURCE_DIR) + String("/TestFiles/");

SCENARIO("voice sources reach the audio thread only through committed snapshots", "[engine][voice][snapshot]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());
    auto engine = AudiumFactory::createAudiumEngine();
    auto tracks = engine->getAudioTrackContainer();
    auto scheduler = engine->getPlayListScheduler();

    auto audioFile = File(voiceTestFilesDirectory + "120-funk-1-sec.wav");
    REQUIRE(audioFile.existsAsFile());

    GIVEN("a clip whose voice source has been committed and pulled") {
        engine->getProjectSerializer()->createNewProject();
        REQUIRE(tracks->addAudioFiles({ audioFile.getFullPathName() }, 0.0, nullptr, false));

        std::shared_ptr<AudioTrack> clipTrack;
        for (auto i = 0; i < tracks->getNumItems(); ++i)
            if (tracks->getAudioTrack(i)->getPlayListContainer()->playListItems.size() > 0)
                clipTrack = tracks->getAudioTrack(i);
        REQUIRE(clipTrack != nullptr);

        auto container = clipTrack->getVoiceSourceContainer();
        auto item = clipTrack->getPlayListContainer()->playListItems.getObjects()[0];
        const auto numVoices = item->getVoiceSources().size(); // one per file channel
        REQUIRE(numVoices >= 1);
        auto voice = item->getVoiceSources()[0];
        std::weak_ptr<VoiceSource> watch = voice;

        const auto index = container->getVoiceSourceIndex(voice);
        REQUIRE(index >= 0);

        scheduler->commitPlayListData();   // message thread
        REQUIRE(container->pull());        // audio thread
        REQUIRE(container->getVoiceSourceAtIndex(index) == voice.get());

        WHEN("the clip releases its voice source on the message thread") {
            auto* raw = voice.get();
            item->deinit();                 // removeVoiceSource + drops the item's reference
            REQUIRE(container->getVoiceSourceIndex(voice) == -1);
            REQUIRE(container->getVoiceSourceAtIndex(index) == raw);

            THEN("the audio thread keeps its snapshot until the next commit is pulled") {
                REQUIRE_FALSE(container->pull());

                scheduler->commitPlayListData();
                REQUIRE(container->pull());
                REQUIRE(container->getVoiceSourceAtIndex(index) == nullptr);
            }

            THEN("the object is retired, not destroyed, until the audio thread let go") {
                voice.reset();
                REQUIRE_FALSE(watch.expired());           // held by the container only
                REQUIRE(container->getNumRetired() == numVoices);

                scheduler->commitPlayListData();          // excluded from this snapshot ...
                REQUIRE_FALSE(watch.expired());           // ... but the consumer has not pulled it
                REQUIRE(container->getNumRetired() == numVoices);

                scheduler->commitPlayListData();          // still not pulled: still alive
                REQUIRE_FALSE(watch.expired());

                REQUIRE(container->pull());               // audio thread takes over the snapshot
                scheduler->commitPlayListData();          // next commit may release it
                REQUIRE(watch.expired());
                REQUIRE(container->getNumRetired() == 0);
            }

            THEN("the freed index is not handed out again") {
                REQUIRE(tracks->addAudioFiles({ audioFile.getFullPathName() }, 5.0, nullptr, false));
                for (auto i = 0; i < tracks->getNumItems(); ++i)
                    for (auto& other : tracks->getAudioTrack(i)->getPlayListContainer()->playListItems.getObjects())
                        for (auto& v : other->getVoiceSources())
                            if (v != nullptr && v.get() != raw) {
                                auto* c = tracks->getAudioTrack(i)->getVoiceSourceContainer().get();
                                if (c == container.get())
                                    REQUIRE(c->getVoiceSourceIndex(v) != index);
                            }
            }
        }
    }

    engine = nullptr;
    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}
