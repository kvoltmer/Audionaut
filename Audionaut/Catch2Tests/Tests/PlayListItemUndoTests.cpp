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
#include "Engine/Undo/UndoablePlayListItemAction.h"

#include "TestUtils.h"

// A clip drag (move, trim, stretch) records a clip-only undo step. The old
// container snapshot replayed the whole project on every drop, and each
// clip's JSON reload recreated its voice sources - a DSP spike per drop and
// a restart of clips that were playing.

using namespace audium;

SCENARIO("clip drags undo without rebuilding the clips", "[engine][undo][playlist]")
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
        auto playList = track->getPlayListContainer();
        auto item = playList->getPlayListItem(0);
        REQUIRE(item != nullptr);
        REQUIRE_FALSE(item->getVoiceSources().empty());

        const auto voiceSourcesBefore = item->getVoiceSources();
        const auto positionBefore = item->getAbsolutePosition(clocks);
        const auto regionBefore = item->getRegionData(clocks);
        auto undoManager = container->getUndoManager();
        const auto undoableBefore = undoManager->canUndo();

        WHEN("the clip is moved as an undo step")
        {
            auto action = std::make_unique<UndoablePlayListItemAction>(*container,
                std::vector<std::shared_ptr<PlayListItem>>{ item });
            item->moveAbsolutePosition(1000.0, clocks);
            action->storeNewState();
            REQUIRE_FALSE(action->isNoOp());
            undoManager->perform(action.release(), "Move Clip");

            THEN("the clip's voice sources are the same objects as before")
            {
                REQUIRE(item->getAbsolutePosition(clocks) == Catch::Approx(positionBefore + 1000.0));
                REQUIRE(item->getVoiceSources() == voiceSourcesBefore);
            }

            THEN("undo moves it back and redo forward, still without a rebuild")
            {
                REQUIRE(undoManager->canUndo());
                undoManager->undo();
                REQUIRE(item->getAbsolutePosition(clocks) == Catch::Approx(positionBefore));
                REQUIRE(item->getVoiceSources() == voiceSourcesBefore);

                undoManager->redo();
                REQUIRE(item->getAbsolutePosition(clocks) == Catch::Approx(positionBefore + 1000.0));
                REQUIRE(item->getVoiceSources() == voiceSourcesBefore);
            }
        }

        WHEN("the clip is trimmed and stretched as one undo step")
        {
            auto action = std::make_unique<UndoablePlayListItemAction>(*container,
                std::vector<std::shared_ptr<PlayListItem>>{ item });
            item->moveAbsoluteStartPosition(250.0, clocks);
            item->setSpeedRatio(1.5);
            action->storeNewState();
            undoManager->perform(action.release(), "Stretch Clip");

            THEN("undo restores the region window and the speed")
            {
                REQUIRE(item->getRegionData(clocks).getStart() == Catch::Approx(regionBefore.getStart() + 250.0));
                REQUIRE(item->getSpeedRatio() == Catch::Approx(1.5));

                undoManager->undo();
                REQUIRE(item->getRegionData(clocks).getStart() == Catch::Approx(regionBefore.getStart()));
                REQUIRE(item->getRegionData(clocks).getLength() == Catch::Approx(regionBefore.getLength()));
                REQUIRE(item->getSpeedRatio() == Catch::Approx(1.0));
                REQUIRE(item->getAbsolutePosition(clocks) == Catch::Approx(positionBefore));
                REQUIRE(item->getVoiceSources() == voiceSourcesBefore);
            }
        }

        WHEN("a drag ends without changing anything")
        {
            auto action = std::make_unique<UndoablePlayListItemAction>(*container,
                std::vector<std::shared_ptr<PlayListItem>>{ item });
            action->storeNewState();

            THEN("the step reports itself as a no-op")
            {
                REQUIRE(action->isNoOp());
                REQUIRE(undoManager->canUndo() == undoableBefore);
            }
        }

        WHEN("a second clip follows the first and the first is moved past it")
        {
            const auto length = item->getAbsolutePositionRange(clocks).getLength();
            auto second = playList->createPlayListItemAtPositionUI(item->getRegion(),
                                                                   positionBefore + length,
                                                                   clocks);
            REQUIRE(second != nullptr);
            second->init();
            REQUIRE(playList->getPlayListItem(0) == item);
            REQUIRE(playList->getPlayListItem(1) == second);

            auto action = std::make_unique<UndoablePlayListItemAction>(*container,
                std::vector<std::shared_ptr<PlayListItem>>{ item });
            item->setAbsolutePosition(positionBefore + 2.0 * length, clocks);
            playList->sortByPosition();
            action->storeNewState();
            undoManager->perform(action.release(), "Move Clip");

            THEN("the playlist order follows the move and undo restores it")
            {
                REQUIRE(playList->getPlayListItem(0) == second);
                REQUIRE(playList->getPlayListItem(1) == item);

                undoManager->undo();
                REQUIRE(playList->getPlayListItem(0) == item);
                REQUIRE(playList->getPlayListItem(1) == second);
                REQUIRE(item->getAbsolutePosition(clocks) == Catch::Approx(positionBefore));

                undoManager->redo();
                REQUIRE(playList->getPlayListItem(0) == second);
                REQUIRE(playList->getPlayListItem(1) == item);
            }
        }

        engine = nullptr;
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}
