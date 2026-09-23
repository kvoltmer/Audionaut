#include <catch2/catch_test_macros.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Project/ProjectSerializer.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Engine/Group/AudioTrackContainer.h"

using namespace audium;

static const auto guardTestFilesDirectory = String(CURRENT_SOURCE_DIR) + String("/TestFiles/");

/** Forces `file`'s mtime `secondsLater` seconds past `reference`, so the
    assertions never race the filesystem's timestamp resolution. */
static void stampRelativeTo(const File& file, const File& reference, int secondsLater)
{
    file.setLastModificationTime(reference.getLastModificationTime() + RelativeTime::seconds(secondsLater));
}

SCENARIO("an external write is only held back when it would discard unsaved edits",
         "[engine][autosave][reload]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());
    auto engine = AudiumFactory::createAudiumEngine();
    auto store = engine->getProjectFileStore();

    auto outProject = File(guardTestFilesDirectory + "Sessions/reload-guard-test.audium/"
                           + ProjectFileStore::projectFileName);
    const auto autosaveFile = outProject.getSiblingFile(ProjectFileStore::autosaveFileName);

    GIVEN("a saved project with no snapshot beside it") {
        engine->getProjectSerializer()->createNewProject();
        REQUIRE(store->save(outProject, nullptr));
        REQUIRE_FALSE(autosaveFile.existsAsFile());

        THEN("a reload cannot lose edits, however dirty the session") {
            REQUIRE_FALSE(ProjectFileStore::reloadWouldLoseEdits(outProject, false));
            REQUIRE_FALSE(ProjectFileStore::reloadWouldLoseEdits(outProject, true));
        }

        WHEN("unsaved edits have been snapshotted since the project file was written") {
            engine->getAudioTrackContainer()->setMasterGain(0.8f);
            REQUIRE(store->writeAutosave());
            stampRelativeTo(autosaveFile, outProject, 2);

            THEN("reloading would discard them, but only while they are unsaved") {
                REQUIRE(ProjectFileStore::reloadWouldLoseEdits(outProject, true));
                REQUIRE_FALSE(ProjectFileStore::reloadWouldLoseEdits(outProject, false));
            }
        }

        WHEN("the incoming write is newer than our snapshot") {
            REQUIRE(store->writeAutosave());
            stampRelativeTo(autosaveFile, outProject, -2);

            THEN("it was made by a writer that had seen the edits, so it reloads") {
                REQUIRE_FALSE(ProjectFileStore::reloadWouldLoseEdits(outProject, true));
            }
        }
    }

    // cleanup ... comment out in case you need to isolate an issue
    outProject.getParentDirectory().deleteRecursively();

    engine = nullptr;
    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}
