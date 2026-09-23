#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Project/ProjectSerializer.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Engine/Group/AudioTrackContainer.h"

using namespace audium;

static const auto snapshotTestFilesDirectory = String(CURRENT_SOURCE_DIR) + String("/TestFiles/");

/** Forces `file`'s mtime `secondsLater` seconds past `reference`, so the
    assertions never race the filesystem's timestamp resolution. */
static void stampRelativeTo(const File& file, const File& reference, int secondsLater)
{
    file.setLastModificationTime(reference.getLastModificationTime() + RelativeTime::seconds(secondsLater));
}

SCENARIO("agents read the crash-recovery snapshot when it is newer", "[engine][autosave][agent]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());
    auto engine = AudiumFactory::createAudiumEngine();
    auto store = engine->getProjectFileStore();

    auto outProject = File(snapshotTestFilesDirectory + "Sessions/agent-snapshot-test.audium/"
                           + ProjectFileStore::projectFileName);
    const auto autosaveFile = outProject.getSiblingFile(ProjectFileStore::autosaveFileName);

    GIVEN("a saved project with no snapshot beside it") {
        engine->getProjectSerializer()->createNewProject();
        engine->getAudioTrackContainer()->setMasterGain(1.0f);
        REQUIRE(store->save(outProject, nullptr));
        REQUIRE_FALSE(autosaveFile.existsAsFile());

        THEN("the project file is the read source") {
            REQUIRE(ProjectFileStore::readSourceFor(outProject) == outProject);
        }

        AND_THEN("a reload cannot lose edits, however dirty the session") {
            REQUIRE_FALSE(ProjectFileStore::reloadWouldLoseEdits(outProject, false));
            REQUIRE_FALSE(ProjectFileStore::reloadWouldLoseEdits(outProject, true));
        }

        WHEN("the GUI takes a snapshot of unsaved edits") {
            engine->getAudioTrackContainer()->setMasterGain(0.8f);
            REQUIRE(store->writeAutosave());
            stampRelativeTo(autosaveFile, outProject, 2);

            THEN("the snapshot becomes the read source") {
                REQUIRE(ProjectFileStore::readSourceFor(outProject) == autosaveFile);
            }

            AND_THEN("reloading would lose those edits, but only while they are unsaved") {
                REQUIRE(ProjectFileStore::reloadWouldLoseEdits(outProject, true));
                REQUIRE_FALSE(ProjectFileStore::reloadWouldLoseEdits(outProject, false));
            }

            AND_WHEN("a reader opens the project without declaring agent intent") {
                auto plainEngine = AudiumFactory::createAudiumEngine();

                REQUIRE(plainEngine->getProjectFileStore()->open(outProject, nullptr));

                THEN("it sees the saved state, leaving the snapshot to crash recovery") {
                    REQUIRE(plainEngine->getAudioTrackContainer()->getMasterGain()
                            == Catch::Approx(1.0f));
                }

                plainEngine = nullptr;
            }

            AND_WHEN("an agent run opens the project") {
                auto agentEngine = AudiumFactory::createAudiumEngine();
                const ProjectFileStore::FollowUnsavedSnapshotScope followUnsaved;

                REQUIRE(agentEngine->getProjectFileStore()->open(outProject, nullptr));

                THEN("it sees the unsaved state, not the saved project file") {
                    REQUIRE(agentEngine->getAudioTrackContainer()->getMasterGain()
                            == Catch::Approx(0.8f));

                    AND_THEN("saving puts the project file back in front") {
                        REQUIRE(agentEngine->getProjectFileStore()->save(outProject, nullptr));
                        stampRelativeTo(outProject, autosaveFile, 2);

                        REQUIRE(ProjectFileStore::readSourceFor(outProject) == outProject);
                        REQUIRE_FALSE(ProjectFileStore::reloadWouldLoseEdits(outProject, true));
                    }
                }

                agentEngine = nullptr;
            }
        }

        WHEN("a stale snapshot sits beside a newer project file") {
            REQUIRE(store->writeAutosave());
            stampRelativeTo(autosaveFile, outProject, -2);

            THEN("the project file stays the read source") {
                REQUIRE(ProjectFileStore::readSourceFor(outProject) == outProject);
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
