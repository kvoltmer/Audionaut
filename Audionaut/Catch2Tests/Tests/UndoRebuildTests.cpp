//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/PlayList/ClipDynamics.h"
#include "Engine/AudioSources/VoiceSource.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Engine/Undo/UndoableContainerAction.h"

#include "TestUtils.h"

// A container undo step replayed without rebuild (clip move, fade, clip
// gain) reuses the playlist items. It must not recreate their voice
// sources: that rebuilds every clip's playback chain and restarts clips
// that are playing.

using namespace audium;

SCENARIO("replaying a container undo step keeps the clips' voice sources", "[engine][undo][playlist]")
{
    GIVEN("a project with one clip on one track")
    {
        auto engine = AudiumFactory::createAudiumEngine();
        const auto audioFile = createSlowSawTwoSecondsAudioFile();
        REQUIRE(audioFile.existsAsFile());
        REQUIRE(engine->getProjectFileStore()->open(audioFile, nullptr));

        auto container = engine->getAudioTrackContainer();
        auto track = container->getAudioTrack(0);
        REQUIRE(track != nullptr);
        auto item = track->getPlayListContainer()->getPlayListItem(0);
        REQUIRE(item != nullptr);
        REQUIRE(item->voiceSourcesMatchRegion());

        const auto voiceSourcesBefore = item->getVoiceSources();
        REQUIRE_FALSE(voiceSourcesBefore.empty());
        auto undoManager = container->getUndoManager();

        WHEN("the clip is moved as a non-rebuilding container step")
        {
            const auto positionBefore = item->getAbsolutePosition(audium::seconds);

            auto action = std::make_unique<UndoableContainerAction>(*container, false);
            item->setAbsolutePosition(positionBefore + 1.0, audium::seconds);
            action->storeNewState();
            undoManager->perform(action.release(), "Move Clip");
            undoManager->beginNewTransaction();

            THEN("the same item keeps the same voice sources through undo and redo")
            {
                REQUIRE(track->getPlayListContainer()->getPlayListItem(0) == item);
                REQUIRE(item->getVoiceSources() == voiceSourcesBefore);

                undoManager->undo();
                REQUIRE(item->getAbsolutePosition(audium::seconds) == Catch::Approx(positionBefore));
                REQUIRE(item->getVoiceSources() == voiceSourcesBefore);

                undoManager->redo();
                REQUIRE(item->getAbsolutePosition(audium::seconds) == Catch::Approx(positionBefore + 1.0));
                REQUIRE(item->getVoiceSources() == voiceSourcesBefore);
            }
        }

        WHEN("the clip's dynamics change as an undo step")
        {
            item->onDragStart();
            item->getDynamics().setGain(0, 0.5);
            item->onDragEnd("Set Clip Gain");

            THEN("undo and redo leave the voice sources alone")
            {
                REQUIRE(item->getVoiceSources() == voiceSourcesBefore);

                undoManager->undo();
                REQUIRE(item->getDynamics().getGain(0) == Catch::Approx(1.0));
                REQUIRE(item->getVoiceSources() == voiceSourcesBefore);

                undoManager->redo();
                REQUIRE(item->getDynamics().getGain(0) == Catch::Approx(0.5));
                REQUIRE(item->getVoiceSources() == voiceSourcesBefore);
            }
        }

        WHEN("a full rebuild is replayed")
        {
            auto action = std::make_unique<UndoableContainerAction>(*container, true);
            item->setAbsolutePosition(2.0, audium::seconds);
            action->storeNewState();
            undoManager->perform(action.release(), "Rebuild");

            THEN("the graph is recreated and the new item has fresh, matching voice sources")
            {
                auto rebuiltTrack = container->getAudioTrack(0);
                REQUIRE(rebuiltTrack != nullptr);
                REQUIRE(rebuiltTrack != track);
                auto rebuilt = rebuiltTrack->getPlayListContainer()->getPlayListItem(0);
                REQUIRE(rebuilt != nullptr);
                REQUIRE(rebuilt != item);
                REQUIRE(rebuilt->voiceSourcesMatchRegion());
                REQUIRE(rebuilt->getVoiceSources() != voiceSourcesBefore);
            }
        }

        engine = nullptr;
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}
