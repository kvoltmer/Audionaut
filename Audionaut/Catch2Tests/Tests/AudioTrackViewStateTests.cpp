//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <catch2/catch_test_macros.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Group/AudioTrackViewState.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Engine/Undo/UndoableContainerAction.h"

#include "TestUtils.h"

// The view state writes its "minimized" flag only when set and used to leave
// the flag alone when the key was absent. An undo snapshot of a track that was
// NOT minimized therefore never cleared the flag, so undoing "minimize track"
// was a no-op. Reading must reset absent keys to their defaults.

using namespace audium;

SCENARIO("track view state restores defaults for keys absent from the JSON", "[engine][undo][viewstate]")
{
    GIVEN("a project with one track")
    {
        auto engine = AudiumFactory::createAudiumEngine();
        const auto audioFile = createSlowSawTwoSecondsAudioFile();
        REQUIRE(audioFile.existsAsFile());
        REQUIRE(engine->getProjectFileStore()->open(audioFile, nullptr));

        auto container = engine->getAudioTrackContainer();
        auto track = container->getAudioTrack(0);
        REQUIRE(track != nullptr);
        auto& viewState = track->getViewState();

        WHEN("a minimized state is written and a JSON without the key is read back into the same object")
        {
            viewState.setMinimized(true);
            viewState.setAnalysisTypeVisible(AnalysisType::Onset, true);

            json minimized;
            REQUIRE(viewState.writeToJson(minimized));
            REQUIRE(minimized.contains("minimized"));
            REQUIRE(minimized["minimized"].get<bool>());

            json plain;
            plain["colour"] = minimized["colour"];
            REQUIRE_FALSE(plain.contains("minimized"));
            REQUIRE_FALSE(plain.contains("visible_analysis"));
            REQUIRE(viewState.readFromJson(plain));

            THEN("the flag and the analysis overlays fall back to their defaults")
            {
                REQUIRE_FALSE(viewState.getMinimized());
                REQUIRE(viewState.getVisibleAnalysisTypes().empty());
                REQUIRE(viewState.getColour().toString().toStdString() == minimized["colour"].get<std::string>());
            }
        }

        WHEN("minimizing the track is recorded as an undo step, like the track header does")
        {
            REQUIRE_FALSE(viewState.getMinimized());
            auto undoManager = container->getUndoManager();

            auto action = std::make_unique<UndoableContainerAction>(*container, false);
            viewState.setMinimized(true);
            action->storeNewState();
            undoManager->perform(action.release(), "minimize audio track");
            undoManager->beginNewTransaction();
            REQUIRE(viewState.getMinimized());

            THEN("undo restores the track to not minimized and redo minimizes it again")
            {
                REQUIRE(undoManager->canUndo());
                undoManager->undo();
                REQUIRE_FALSE(viewState.getMinimized());

                undoManager->redo();
                REQUIRE(viewState.getMinimized());
            }
        }

        engine = nullptr;
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}
