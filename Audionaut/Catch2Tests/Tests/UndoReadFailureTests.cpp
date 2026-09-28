//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <catch2/catch_test_macros.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Group/ResourceGroup.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Engine/Resource/AudioResource.h"
#include "Engine/Undo/UndoableEdit.h"

#include "TestUtils.h"

// A container undo step whose stored state can't be re-applied must report
// the failure: the UndoManager then drops the step instead of recording a
// transaction that never took, and the edit's caller learns it didn't happen.

using namespace audium;

SCENARIO("an undoable edit whose state can't be re-applied reports failure", "[engine][undo][container]")
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

        auto groups = track->getResourceGroups();
        REQUIRE_FALSE(groups.empty());
        auto resources = groups[0]->getAudioResources();
        REQUIRE_FALSE(resources.empty());
        const auto mediaFile = resources[0]->getLocalFile();
        REQUIRE(mediaFile.existsAsFile());
        const auto movedAway = mediaFile.getSiblingFile(mediaFile.getFileName() + ".moved");

        auto undoManager = container->getUndoManager();
        undoManager->clearUndoHistory();

        WHEN("the edit's media file disappears before the recorded state is applied")
        {
            // perform() re-reads the new snapshot with a rebuild, which
            // re-opens every media file - a missing one makes it throw
            const auto applied = applyAsUndoableEdit(*container, [&]
            {
                return mediaFile.moveFileTo(movedAway);
            }, "Vanishing Edit");

            REQUIRE(movedAway.moveFileTo(mediaFile));

            THEN("the edit reports failure and records no undo step")
            {
                REQUIRE_FALSE(applied);
                REQUIRE_FALSE(undoManager->canUndo());
            }
        }

        track = nullptr;
        container = nullptr;
        engine = nullptr;
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}
