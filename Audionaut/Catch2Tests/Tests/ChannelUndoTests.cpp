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
#include "Engine/AudioSources/VoiceSource.h"
#include "Engine/Project/ProjectFileStore.h"

#include "TestUtils.h"

// Mixer parameters (gain, pan, mute, solo, monitor) record a channel-only
// undo step. The old container snapshot replayed the whole track on every
// click, recreating each clip's playback chain - a CPU spike per mute or
// solo press and a restart of clips that were playing.

using namespace audium;

SCENARIO("channel mixer parameters undo without rebuilding the track", "[engine][undo][channel]")
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
        REQUIRE_FALSE(item->getVoiceSources().empty());

        const auto voiceSourcesBefore = item->getVoiceSources();
        auto undoManager = container->getUndoManager();
        const auto undoableBefore = undoManager->canUndo();

        WHEN("solo is toggled as an undo step")
        {
            track->onDragStart(0);
            track->setSolo(true, 0);
            track->onDragEnd("Solo");

            THEN("the clip's voice sources are the same objects as before")
            {
                REQUIRE(item->getVoiceSources() == voiceSourcesBefore);
            }

            THEN("undo clears the solo and redo re-applies it, still without a rebuild")
            {
                REQUIRE(undoManager->canUndo());
                undoManager->undo();
                REQUIRE_FALSE(track->getSolo(0));
                REQUIRE(item->getVoiceSources() == voiceSourcesBefore);

                undoManager->redo();
                REQUIRE(track->getSolo(0));
                REQUIRE(item->getVoiceSources() == voiceSourcesBefore);
            }
        }

        WHEN("mute is toggled as an undo step")
        {
            track->onDragStart(0);
            track->setMute(true, 0);
            track->onDragEnd("Mute");

            THEN("undo restores the unmuted channel")
            {
                REQUIRE(track->getMute(0));
                undoManager->undo();
                REQUIRE_FALSE(track->getMute(0));
                REQUIRE(item->getVoiceSources() == voiceSourcesBefore);
            }
        }

        WHEN("a gain drag ends")
        {
            const auto gainBefore = track->getGain(0);

            track->onDragStart(0);
            track->setGain(0.25f, 0);
            track->setGain(0.5f, 0);
            track->onDragEnd("Set Gain");

            THEN("one undo step takes the gain back to where the drag started")
            {
                REQUIRE(track->getGain(0) == Catch::Approx(0.5f));
                undoManager->undo();
                REQUIRE(track->getGain(0) == Catch::Approx(gainBefore));
                REQUIRE(undoManager->canUndo() == undoableBefore);
            }
        }

        WHEN("a pan drag ends")
        {
            track->onDragStart(0);
            track->setPan(-0.75f, 0);
            track->onDragEnd("Set Pan");

            THEN("undo and redo move the pan back and forth")
            {
                undoManager->undo();
                REQUIRE(track->getPan(0) == Catch::Approx(0.0f));
                undoManager->redo();
                REQUIRE(track->getPan(0) == Catch::Approx(-0.75f));
            }
        }

        WHEN("a drag ends without changing anything")
        {
            track->onDragStart(0);
            track->onDragEnd("Set Gain");

            THEN("no undo step is recorded")
            {
                REQUIRE(undoManager->canUndo() == undoableBefore);
                REQUIRE_FALSE(undoManager->canRedo());
            }
        }

        WHEN("onDragEnd is called without a matching onDragStart")
        {
            track->onDragEnd("Set Gain");

            THEN("it is a no-op")
            {
                REQUIRE(undoManager->canUndo() == undoableBefore);
            }
        }

        engine = nullptr;
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}
