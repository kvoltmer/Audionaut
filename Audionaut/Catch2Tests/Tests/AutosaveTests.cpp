#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Group/AudioTrackContainer.h"

#if !JUCE_WINDOWS
 #include <unistd.h>
#endif

using namespace audium;

static int currentTestProcessId()
{
#if JUCE_WINDOWS
    return (int) GetCurrentProcessId();
#else
    return (int) getpid();
#endif
}

static const auto autosaveTestFilesDirectory = String(CURRENT_SOURCE_DIR) + String("/TestFiles/");

SCENARIO("autosave writes a snapshot without touching the project file", "[engine][autosave]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());
    auto engine = AudiumFactory::createAudiumEngine();

    auto outProject = File(autosaveTestFilesDirectory + "Sessions/autosave-test.audium/" + AudiumEngine::projectFileName);

    GIVEN("a never-saved project") {
        engine->createNewProject();

        THEN("the snapshot goes to the session's temp directory, with a pid file") {
            REQUIRE(engine->writeAutosave());

            const auto tempAutosave = AudiumEngine::tempDirectory.getChildFile(AudiumEngine::autosaveFileName);
            const auto tempPidFile = AudiumEngine::tempDirectory.getChildFile(AudiumEngine::autosavePidFileName);
            REQUIRE(tempAutosave.existsAsFile());
            REQUIRE(tempPidFile.existsAsFile());

            AND_THEN("deleteAutosave removes both") {
                engine->deleteAutosave();
                REQUIRE_FALSE(tempAutosave.existsAsFile());
                REQUIRE_FALSE(tempPidFile.existsAsFile());
            }
        }

        engine->deleteAutosave();
    }

    GIVEN("a saved project") {
        engine->createNewProject();
        REQUIRE(engine->saveFile(outProject, nullptr));

        const auto autosaveFile = outProject.getSiblingFile(AudiumEngine::autosaveFileName);
        const auto projectMtimeBefore = outProject.getLastModificationTime();

        WHEN("an autosave is written") {
            engine->getAudioTrackContainer()->setMasterGain(0.8f);
            REQUIRE(engine->writeAutosave());

            THEN("the snapshot exists and Project.json, stamps and undo are untouched") {
                REQUIRE(autosaveFile.existsAsFile());
                REQUIRE(outProject.getLastModificationTime() == projectMtimeBefore);
                REQUIRE_FALSE(engine->projectChangedOnDisk());
                REQUIRE_FALSE(engine->getUndoManager()->canUndo());
            }

            AND_WHEN("the project is saved cleanly") {
                REQUIRE(engine->saveFile(outProject, nullptr));

                THEN("the stale snapshot is deleted") {
                    REQUIRE_FALSE(autosaveFile.existsAsFile());
                }
            }

            AND_WHEN("the project is saved-as to another package") {
                auto otherProject = File(autosaveTestFilesDirectory
                                         + "Sessions/autosave-saveas-test.audium/"
                                         + AudiumEngine::projectFileName);
                REQUIRE(engine->saveFile(otherProject, nullptr));

                THEN("the original package's snapshot is gone too") {
                    REQUIRE_FALSE(autosaveFile.existsAsFile());
                }

                otherProject.getParentDirectory().deleteRecursively();
            }
        }
    }

    // cleanup ... comment out in case you need to isolate an issue
    outProject.getParentDirectory().deleteRecursively();

    engine = nullptr;
    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("orphaned temp autosaves are found, live sessions are left alone", "[engine][autosave][orphan]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());
    auto engine = AudiumFactory::createAudiumEngine();
    engine->createNewProject();

    // leftovers from previous test runs must not leak into the assertions
    for (auto stale = AudiumEngine::findOrphanedTempAutosave(); stale != File();
         stale = AudiumEngine::findOrphanedTempAutosave())
        stale.deleteRecursively();

    const auto tempRoot = File::getSpecialLocation(File::tempDirectory);
    const auto orphan = tempRoot.getChildFile("temp-orphantest" + String(AudiumEngine::projectFileExtension));
    REQUIRE(orphan.createDirectory());

    GIVEN("a temp package with an autosave and no owning process") {
        REQUIRE(orphan.getChildFile(AudiumEngine::autosaveFileName).replaceWithText("{}"));

        THEN("the scan finds it") {
            REQUIRE(AudiumEngine::findOrphanedTempAutosave() == orphan);
        }

        WHEN("its pid file names a live process (this one)") {
            orphan.getChildFile(AudiumEngine::autosavePidFileName)
                  .replaceWithText(String(currentTestProcessId()));

            THEN("the scan leaves it alone") {
                REQUIRE(AudiumEngine::findOrphanedTempAutosave() == File());
            }
        }

        WHEN("its pid file names a dead process") {
            // pid_max on macOS is 99998, so this can never be alive
            orphan.getChildFile(AudiumEngine::autosavePidFileName).replaceWithText("999999");

            THEN("the scan claims it") {
                REQUIRE(AudiumEngine::findOrphanedTempAutosave() == orphan);
            }
        }

        WHEN("its Project.json is newer than the autosave (already restored)") {
            orphan.getChildFile(AudiumEngine::projectFileName).replaceWithText("{}");

            THEN("the scan leaves it alone") {
                REQUIRE(AudiumEngine::findOrphanedTempAutosave() == File());
            }
        }
    }

    GIVEN("the session's own temp directory holds an autosave") {
        REQUIRE(engine->writeAutosave());

        THEN("the scan skips it - a pid file of a live process guards it, and it is the current session") {
            const auto found = AudiumEngine::findOrphanedTempAutosave();
            REQUIRE(found != AudiumEngine::tempDirectory);
        }

        engine->deleteAutosave();
    }

    orphan.deleteRecursively();

    engine = nullptr;
    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("a never-saved project with audio restores from its temp package", "[engine][autosave][orphan][restore]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());
    auto engine = AudiumFactory::createAudiumEngine();

    const auto audioFile = File(autosaveTestFilesDirectory + "silence-fade.aiff");
    REQUIRE(audioFile.existsAsFile());

    GIVEN("a never-saved project with an audio file, autosaved to the temp directory") {
        engine->createNewProject();
        REQUIRE(engine->openFile(audioFile, nullptr));
        REQUIRE(engine->writeAutosave());

        const auto orphanDir = AudiumEngine::tempDirectory;

        THEN("resource paths are relative to the temp package, not garbage") {
            FileInputStream in(orphanDir.getChildFile(AudiumEngine::autosaveFileName));
            REQUIRE(in.openedOk());
            auto j = json::parse(in.readString().toStdString());
            const auto relPath = String(j["audium"]["audio_tracks"][1]["resource_groups"][0]["resources"][0]
                                            ["relative_file_path"].template get<std::string>())
                                     .replaceCharacter('\\', '/'); // Windows writes native separators
            REQUIRE(relPath == "Media/Audio/" + audioFile.getFileName());
        }

        WHEN("the session crashes and a fresh one finds and restores the orphan") {
            // simulate the crash + relaunch: the temp directory is no longer
            // this session's, and its owning process is gone
            AudiumEngine::tempDirectory = File();
            orphanDir.getChildFile(AudiumEngine::autosavePidFileName).replaceWithText("999999");

            REQUIRE(AudiumEngine::findOrphanedTempAutosave() == orphanDir);

            // promote the snapshot and open the temp package, as the app does
            REQUIRE(orphanDir.getChildFile(AudiumEngine::autosaveFileName)
                        .copyFileTo(orphanDir.getChildFile(AudiumEngine::projectFileName)));
            REQUIRE(engine->openFile(orphanDir, nullptr));

            THEN("the audio track is back") {
                REQUIRE(engine->getAudioTrackContainer()->getNumItems() == 2);
            }

            orphanDir.deleteRecursively();
        }

        // cleanup for the non-crash branch
        if (orphanDir.exists() && orphanDir == AudiumEngine::tempDirectory)
            engine->deleteAutosave();
    }

    engine = nullptr;
    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("a crash-recovery snapshot restores as a dirty, undoable session", "[engine][autosave][restore][undo]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());
    auto engine = AudiumFactory::createAudiumEngine();

    auto outProject = File(autosaveTestFilesDirectory + "Sessions/autosave-restore-test.audium/" + AudiumEngine::projectFileName);

    GIVEN("a project whose last session autosaved unsaved changes and then crashed") {
        engine->createNewProject();
        REQUIRE(engine->saveFile(outProject, nullptr));

        engine->getAudioTrackContainer()->setMasterGain(0.8f);
        REQUIRE(engine->writeAutosave());

        // "crash": reopen the project from disk - the unsaved edit is gone
        REQUIRE(engine->openFile(outProject.getParentDirectory(), nullptr));
        REQUIRE(engine->getAudioTrackContainer()->getMasterGain() == Catch::Approx(1.0));
        REQUIRE_FALSE(engine->getUndoManager()->canUndo());

        WHEN("the snapshot is restored") {
            REQUIRE(engine->restoreAutosave(nullptr));

            THEN("the session is dirty and undo returns to the saved state") {
                REQUIRE(engine->getAudioTrackContainer()->getMasterGain() == Catch::Approx(0.8));
                REQUIRE(engine->getUndoManager()->canUndo());

                REQUIRE(engine->getUndoManager()->undo());
                REQUIRE(engine->getAudioTrackContainer()->getMasterGain() == Catch::Approx(1.0));
            }
        }
    }

    // cleanup ... comment out in case you need to isolate an issue
    outProject.getParentDirectory().deleteRecursively();

    engine = nullptr;
    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}
