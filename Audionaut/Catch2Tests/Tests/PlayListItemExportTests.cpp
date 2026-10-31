//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <catch2/catch_test_macros.hpp>

#include "Engine/Export/PlayListItemExport.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/Project/ProjectFileStore.h"

#include "TestUtils.h"
#include "TestEngine.h"

using namespace audium;
using namespace juce;

namespace
{
    // Fires only when something runs the message loop.
    struct LoopWitness final : Timer
    {
        LoopWitness() { startTimer (1); }
        ~LoopWitness() override { stopTimer(); }
        void timerCallback() override { ++ticks; }
        int ticks = 0;
    };
}

// The chooser-less clip export backs dragging a clip out of the window:
// JUCE asks for the files from inside its drag-image mouse handling, and
// anything that pumps the message loop there lets the drag image delete
// itself before JUCE deletes it again (#245). So the export has to run to
// completion on the calling thread, with no loop underneath it.
SCENARIO("clip export without a file chooser", "[engine][export]")
{
    auto inputFile = generateDcOffsetAudioFile (1.0);

    TestEngine engine;
    REQUIRE (engine.open (inputFile));
    engine->getPlayListScheduler()->commitPlayListData();
    REQUIRE (engine->getAudioResourceContainer()->createTemporaryProjectDirectory (false));

    auto items = engine.track (0)->getPlayListContainer()->getPlayListItems();
    REQUIRE (items.size() == 1);

    GIVEN("a clip on the first track")
    {
        WHEN("it is exported for an external drag")
        {
            LoopWitness loop;
            PlayListItemExport exporter (engine.ptr(), items[0], false);
            auto written = exporter.exportItem();

            THEN("the file is in the temporary project directory when the call returns")
            {
                REQUIRE (written);
                REQUIRE (exporter.config != nullptr);
                REQUIRE (exporter.config->fileName.existsAsFile());
                REQUIRE (exporter.config->fileName.isAChildOf (engine->getProjectFileStore()->getTemporaryDirectory()));
            }

            THEN("no message loop ran underneath the export")
            {
                REQUIRE (loop.ticks == 0);
            }
        }
    }

    if (inputFile.existsAsFile())
        inputFile.deleteFile();
}
