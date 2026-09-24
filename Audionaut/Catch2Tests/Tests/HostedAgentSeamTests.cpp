#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Project/ProjectSerializer.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Resource/AudioResourceContainer.h"

using namespace audium;

static const auto seamTestFilesDirectory = String(CURRENT_SOURCE_DIR) + String("/TestFiles/");

/**
 * The seams a hosted agent session needs: apply a state computed elsewhere as
 * one undoable step, and keep a second engine in the process from taking the
 * live session's temp directory with it when it goes.
 */
SCENARIO("a project state computed elsewhere applies as one undoable step",
         "[engine][hosted][undo]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());
    auto engine = AudiumFactory::createAudiumEngine();
    auto store = engine->getProjectFileStore();
    auto container = engine->getAudioTrackContainer();

    auto outProject = File(seamTestFilesDirectory + "Sessions/hosted-seam-test.audium/"
                           + ProjectFileStore::projectFileName);

    GIVEN("a saved project") {
        engine->getProjectSerializer()->createNewProject();
        container->setMasterGain(1.0f);
        REQUIRE(store->save(outProject, nullptr));

        const auto projectWrittenAt = outProject.getLastModificationTime();

        WHEN("a state carrying a different gain is applied") {
            // stands in for the state a scratch engine would hand back
            json afterState;
            engine->getProjectSerializer()->writeToJson(afterState);
            afterState["audium"]["master_gain"] = 0.25;

            REQUIRE(store->applyStateAsUndoableReload(afterState, true, true,
                                                      "Agent: test", nullptr));

            THEN("the graph carries it, the file does not, and one undo reverses it") {
                REQUIRE(container->getMasterGain() == Catch::Approx(0.25f));
                REQUIRE(outProject.getLastModificationTime() == projectWrittenAt);
                REQUIRE(store->wasChangedExternally());
                REQUIRE(engine->getUndoManager()->canUndo());

                REQUIRE(engine->getUndoManager()->undo());
                REQUIRE(container->getMasterGain() == Catch::Approx(1.0f));
                REQUIRE_FALSE(store->wasChangedExternally());
            }
        }

        WHEN("the applied state is the one already loaded") {
            json sameState;
            engine->getProjectSerializer()->writeToJson(sameState);

            THEN("applying it still succeeds and leaves the file alone") {
                REQUIRE(store->applyStateAsUndoableReload(sameState, true, true,
                                                          "Agent: no-op", nullptr));
                REQUIRE(container->getMasterGain() == Catch::Approx(1.0f));
                REQUIRE(outProject.getLastModificationTime() == projectWrittenAt);
            }
        }
    }

    // cleanup ... comment out in case you need to isolate an issue
    outProject.getParentDirectory().deleteRecursively();

    engine = nullptr;
    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("a second engine leaves the session's temp directory alone",
         "[engine][hosted][temp]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    auto engine = AudiumFactory::createAudiumEngine();
    engine->getProjectSerializer()->createNewProject();

    const auto sessionTemp = ProjectFileStore::tempDirectory;
    REQUIRE(sessionTemp != File());
    REQUIRE(sessionTemp.isDirectory());

    GIVEN("a scratch engine that does not own the temp directory") {
        {
            auto scratch = AudiumFactory::createAudiumEngine();
            scratch->getAudioResourceContainer()->setOwnsTemporaryDirectory(false);
            REQUIRE(ProjectFileStore::tempDirectory == sessionTemp);
        }

        THEN("its teardown leaves the directory and the static in place") {
            REQUIRE(ProjectFileStore::tempDirectory == sessionTemp);
            REQUIRE(sessionTemp.isDirectory());
        }
    }

    GIVEN("a second engine that does own it (the default)") {
        {
            auto owning = AudiumFactory::createAudiumEngine();
            REQUIRE(ProjectFileStore::tempDirectory == sessionTemp);
        }

        THEN("its teardown takes the shared directory with it") {
            // documents why the ownership flag has to exist
            REQUIRE(ProjectFileStore::tempDirectory == File());
            REQUIRE_FALSE(sessionTemp.isDirectory());
        }
    }

    engine = nullptr;
    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}
