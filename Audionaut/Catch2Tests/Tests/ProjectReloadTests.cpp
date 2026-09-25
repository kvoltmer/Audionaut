#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Project/ProjectSerializer.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Analysis/AnalysisCache.h"
#include "Engine/Export/AudioExporter.h"
#include "Engine/PlayList/PlayListScheduler.h"

#include "TestUtils.h"

#include <optional>

using namespace audium;

static const auto reloadTestFilesDirectory = String(CURRENT_SOURCE_DIR) + String("/TestFiles/");

// Project.json is framed by juce::OutputStream::writeString (see Streamable),
// so external writers in these tests read/write it the same way.
static json readProjectJson(const File& file)
{
    FileInputStream in(file);
    REQUIRE(in.openedOk());
    return json::parse(in.readString().toStdString());
}

static void writeProjectJsonExternally(const File& file, const json& j)
{
    TemporaryFile temp(file);
    {
        auto out = std::unique_ptr<FileOutputStream>(temp.getFile().createOutputStream());
        REQUIRE(out != nullptr);
        REQUIRE_FALSE(out->failedToOpen());
        out->writeString(j.dump(2));
        out->flush();
    }
    REQUIRE(temp.overwriteTargetFileWithTemporary());

    // make sure the mtime differs from the app's stamp even on coarse clocks
    file.setLastModificationTime(Time::getCurrentTime() + RelativeTime::seconds(2));
}

SCENARIO("external change reloads as an undoable step", "[engine][reload][undo]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());
    auto engine = AudiumFactory::createAudiumEngine();
    auto store = engine->getProjectFileStore();

    auto outProject = File(reloadTestFilesDirectory + "Sessions/reload-test.audium/" + ProjectFileStore::projectFileName);

    GIVEN("a saved project") {
        engine->getProjectSerializer()->createNewProject();
        REQUIRE(store->save(outProject, nullptr));
        REQUIRE(engine->getAudioTrackContainer()->getMasterGain() == Catch::Approx(1.0));

        WHEN("an external writer changes the master gain on disk") {
            auto j = readProjectJson(outProject);
            j["audium"]["master_gain"] = 0.5;
            writeProjectJsonExternally(outProject, j);

            REQUIRE(store->projectChangedOnDisk());
            REQUIRE(store->reloadFromDisk(nullptr));

            THEN("the reload applies the external state as an undo step") {
                REQUIRE(engine->getAudioTrackContainer()->getMasterGain() == Catch::Approx(0.5));
                REQUIRE(engine->getUndoManager()->canUndo());
                REQUIRE(store->wasChangedExternally());

                AND_THEN("undo restores memory but never touches the disk") {
                    REQUIRE(engine->getUndoManager()->undo());
                    REQUIRE(engine->getAudioTrackContainer()->getMasterGain() == Catch::Approx(1.0));
                    REQUIRE(readProjectJson(outProject)["audium"]["master_gain"].template get<double>() == Catch::Approx(0.5));

                    // the marker follows the undo stack
                    REQUIRE_FALSE(store->wasChangedExternally());

                    REQUIRE(engine->getUndoManager()->redo());
                    REQUIRE(engine->getAudioTrackContainer()->getMasterGain() == Catch::Approx(0.5));
                    REQUIRE(store->wasChangedExternally());
                }

                AND_THEN("a save clears the external-change marker") {
                    REQUIRE(store->save(outProject, nullptr));
                    REQUIRE_FALSE(store->wasChangedExternally());
                }
            }
        }
    }

    // cleanup ... comment out in case you need to isolate an issue
    outProject.getParentDirectory().deleteRecursively();

    engine = nullptr;
    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("external track addition survives reload, undo and redo", "[engine][reload][undo][rebuild]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());
    auto engine = AudiumFactory::createAudiumEngine();
    auto store = engine->getProjectFileStore();

    auto outProject = File(reloadTestFilesDirectory + "Sessions/reload-track-test.audium/" + ProjectFileStore::projectFileName);

    GIVEN("a saved single-track project") {
        engine->getProjectSerializer()->createNewProject();
        REQUIRE(store->save(outProject, nullptr));
        REQUIRE(engine->getAudioTrackContainer()->getNumItems() == 1);

        WHEN("an external writer adds a second track on disk") {
            auto j = readProjectJson(outProject);
            auto secondTrack = j["audium"]["audio_tracks"][0];
            secondTrack["name"] = "Agent Track";
            j["audium"]["audio_tracks"].push_back(secondTrack);
            writeProjectJsonExternally(outProject, j);

            REQUIRE(store->reloadFromDisk(nullptr));

            THEN("the track count changes and round-trips across undo/redo") {
                REQUIRE(engine->getAudioTrackContainer()->getNumItems() == 2);

                REQUIRE(engine->getUndoManager()->undo());
                REQUIRE(engine->getAudioTrackContainer()->getNumItems() == 1);

                REQUIRE(engine->getUndoManager()->redo());
                REQUIRE(engine->getAudioTrackContainer()->getNumItems() == 2);
            }
        }
    }

    // cleanup ... comment out in case you need to isolate an issue
    outProject.getParentDirectory().deleteRecursively();

    engine = nullptr;
    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("disk stamps tell the app's own writes apart from foreign ones", "[engine][reload][stamps]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());
    auto engine = AudiumFactory::createAudiumEngine();
    auto store = engine->getProjectFileStore();

    auto outProject = File(reloadTestFilesDirectory + "Sessions/stamp-test.audium/" + ProjectFileStore::projectFileName);

    GIVEN("a saved project") {
        engine->getProjectSerializer()->createNewProject();
        REQUIRE(store->save(outProject, nullptr));

        THEN("the app's own save does not read as an external change") {
            REQUIRE_FALSE(store->projectChangedOnDisk());
        }

        WHEN("the file is rewritten externally") {
            writeProjectJsonExternally(outProject, readProjectJson(outProject));

            THEN("the change is detected until the reload refreshes the stamps") {
                REQUIRE(store->projectChangedOnDisk());
                REQUIRE(store->reloadFromDisk(nullptr));
                REQUIRE_FALSE(store->projectChangedOnDisk());
            }
        }
    }

    // cleanup ... comment out in case you need to isolate an issue
    outProject.getParentDirectory().deleteRecursively();

    engine = nullptr;
    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("the agent marker survives undoing only the newest of two reloads", "[engine][reload][undo][marker]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());
    auto engine = AudiumFactory::createAudiumEngine();
    auto store = engine->getProjectFileStore();

    auto outProject = File(reloadTestFilesDirectory + "Sessions/reload-marker-test.audium/" + ProjectFileStore::projectFileName);

    GIVEN("two consecutive external changes, each reloaded") {
        engine->getProjectSerializer()->createNewProject();
        REQUIRE(store->save(outProject, nullptr));

        auto j = readProjectJson(outProject);
        j["audium"]["master_gain"] = 0.5;
        writeProjectJsonExternally(outProject, j);
        REQUIRE(store->reloadFromDisk(nullptr));

        j["audium"]["master_gain"] = 0.25;
        writeProjectJsonExternally(outProject, j);
        REQUIRE(store->reloadFromDisk(nullptr));

        REQUIRE(engine->getAudioTrackContainer()->getMasterGain() == Catch::Approx(0.25));
        REQUIRE(store->wasChangedExternally());

        WHEN("only the newest reload is undone") {
            REQUIRE(engine->getUndoManager()->undo());

            THEN("the first agent version is active, so the marker stays") {
                REQUIRE(engine->getAudioTrackContainer()->getMasterGain() == Catch::Approx(0.5));
                REQUIRE(store->wasChangedExternally());

                AND_THEN("undoing the first reload finally clears it") {
                    REQUIRE(engine->getUndoManager()->undo());
                    REQUIRE(engine->getAudioTrackContainer()->getMasterGain() == Catch::Approx(1.0));
                    REQUIRE_FALSE(store->wasChangedExternally());
                }
            }
        }
    }

    // cleanup ... comment out in case you need to isolate an issue
    outProject.getParentDirectory().deleteRecursively();

    engine = nullptr;
    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("undoing a reload restores unsaved local edits, not the saved state", "[engine][reload][undo][dirty]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());
    auto engine = AudiumFactory::createAudiumEngine();
    auto store = engine->getProjectFileStore();

    auto outProject = File(reloadTestFilesDirectory + "Sessions/reload-dirty-test.audium/" + ProjectFileStore::projectFileName);

    GIVEN("a saved project with an unsaved local edit") {
        engine->getProjectSerializer()->createNewProject();
        REQUIRE(store->save(outProject, nullptr));

        engine->getAudioTrackContainer()->setMasterGain(0.7f);

        WHEN("an external writer changes the file and the app reloads") {
            auto j = readProjectJson(outProject);
            j["audium"]["master_gain"] = 0.5;
            writeProjectJsonExternally(outProject, j);

            REQUIRE(store->reloadFromDisk(nullptr));
            REQUIRE(engine->getAudioTrackContainer()->getMasterGain() == Catch::Approx(0.5));

            THEN("undo returns to the pre-reload in-memory state") {
                REQUIRE(engine->getUndoManager()->undo());
                REQUIRE(engine->getAudioTrackContainer()->getMasterGain() == Catch::Approx(0.7));
            }
        }
    }

    // cleanup ... comment out in case you need to isolate an issue
    outProject.getParentDirectory().deleteRecursively();

    engine = nullptr;
    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("a corrupt analysis sidecar never blocks reload or open", "[engine][reload][analysis]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());
    auto engine = AudiumFactory::createAudiumEngine();
    auto store = engine->getProjectFileStore();

    auto outProject = File(reloadTestFilesDirectory + "Sessions/reload-analysis-test.audium/" + ProjectFileStore::projectFileName);

    GIVEN("a saved project") {
        engine->getProjectSerializer()->createNewProject();
        REQUIRE(store->save(outProject, nullptr));

        WHEN("an external writer leaves a truncated AnalysisData.json next to it") {
            auto sidecar = outProject.getSiblingFile(AnalysisCache::fileName);
            REQUIRE(sidecar.replaceWithText("{ \"version\": 1, \"entries\": [ truncated"));
            sidecar.setLastModificationTime(Time::getCurrentTime() + RelativeTime::seconds(2));
            REQUIRE(store->analysisChangedOnDisk());

            THEN("the reload path (what the project monitor's timer calls) does not throw") {
                REQUIRE_NOTHROW(store->reloadAnalysisFromDisk());
                REQUIRE_NOTHROW(store->reloadFromDisk(nullptr));
            }

            THEN("the project still opens") {
                bool opened = false;
                REQUIRE_NOTHROW(opened = store->open(outProject, nullptr));
                REQUIRE(opened);
                REQUIRE(engine->getAudioTrackContainer()->getNumItems() == 1);
            }
        }
    }

    // cleanup ... comment out in case you need to isolate an issue
    outProject.getParentDirectory().deleteRecursively();

    engine = nullptr;
    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("external channel removal reloads in place without duplicating resource groups", "[engine][reload][rebuild]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());
    auto engine = AudiumFactory::createAudiumEngine();
    auto store = engine->getProjectFileStore();

    auto outProject = File(reloadTestFilesDirectory + "Sessions/reload-channel-test.audium/" + ProjectFileStore::projectFileName);
    auto audioFile = File(reloadTestFilesDirectory + "120-funk-1-sec.wav");
    REQUIRE(audioFile.existsAsFile());

    GIVEN("a saved project whose clip track has a spare channel") {
        engine->getProjectSerializer()->createNewProject();
        auto tracks = engine->getAudioTrackContainer();
        REQUIRE(tracks->addAudioFiles({ audioFile.getFullPathName() }, 0.0, nullptr, false));

        std::shared_ptr<AudioTrack> clipTrack;
        for (auto i = 0; i < tracks->getNumItems(); ++i)
            if (! tracks->getAudioTrack(i)->getResourceGroups().empty())
                clipTrack = tracks->getAudioTrack(i);
        REQUIRE(clipTrack != nullptr);

        clipTrack->ensureNumChannels(clipTrack->getNumAudioTrackChannels() + 1);
        const auto trackIndex = clipTrack->getId();
        const auto numChannels = clipTrack->getNumAudioTrackChannels();
        const auto numGroups = clipTrack->getResourceGroups().size();
        const auto numResources = engine->getAudioResourceContainer()->getNumAudioResources();
        const auto numClips = clipTrack->getPlayListContainer()->playListItems.size();
        REQUIRE(numGroups == 1);
        REQUIRE(store->save(outProject, nullptr));

        WHEN("an external writer drops the spare channel on disk") {
            auto j = readProjectJson(outProject);
            auto& channels = j["audium"]["audio_tracks"][(size_t) trackIndex]["channels"];
            REQUIRE(channels.size() == (size_t) numChannels);
            channels.erase(channels.size() - 1);
            writeProjectJsonExternally(outProject, j);

            REQUIRE(store->reloadFromDisk(nullptr));

            THEN("the channel goes away and the groups, resources and clips are unchanged") {
                auto track = tracks->getAudioTrack(trackIndex);
                REQUIRE(track->getNumAudioTrackChannels() == numChannels - 1);
                REQUIRE(track->getResourceGroups().size() == numGroups);
                REQUIRE(engine->getAudioResourceContainer()->getNumAudioResources() == numResources);
                REQUIRE(track->getPlayListContainer()->playListItems.size() == numClips);

                AND_THEN("a save persists a single group") {
                    REQUIRE(store->save(outProject, nullptr));
                    auto saved = readProjectJson(outProject);
                    REQUIRE(saved["audium"]["audio_tracks"][(size_t) trackIndex]["resource_groups"].size() == numGroups);
                }
            }
        }
    }

    // cleanup ... comment out in case you need to isolate an issue
    outProject.getParentDirectory().deleteRecursively();

    engine = nullptr;
    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

// The app's export and stem separation render on a worker thread behind a
// modal progress window whose loop keeps the project monitor's timer going.
// A reload landing then would rebuild the graph the render is walking.
SCENARIO("an external change waits for a running render", "[engine][reload][render]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());
    auto engine = AudiumFactory::createAudiumEngine();
    auto store = engine->getProjectFileStore();
    auto scheduler = engine->getPlayListScheduler();

    auto inputFile = generateDcOffsetAudioFile(1.0);
    auto outProject = File(reloadTestFilesDirectory + "Sessions/reload-render-test.audium/" + ProjectFileStore::projectFileName);

    auto bounceConfig = std::make_shared<ExportAudioConfig>();
    bounceConfig->fileName = File(reloadTestFilesDirectory + "reload-render-bounce.wav");
    bounceConfig->sampleRate = 44100.0;
    bounceConfig->blockSize = 512;
    bounceConfig->numChannels = 1;

    GIVEN("a saved project with a clip, changed on disk by an external writer") {
        REQUIRE(store->open(inputFile, nullptr));
        REQUIRE(store->save(outProject, nullptr));
        REQUIRE(engine->getAudioTrackContainer()->getMasterGain() == Catch::Approx(1.0));

        auto j = readProjectJson(outProject);
        j["audium"]["master_gain"] = 0.5;
        writeProjectJsonExternally(outProject, j);
        REQUIRE(store->projectChangedOnDisk());

        WHEN("the project is bounced and the reload is asked for mid-render") {
            // What the monitor would do from the modal progress loop, asked
            // from inside the bounce's progress callback: the one place where
            // "the render is running" is certain without a second thread.
            std::optional<bool> renderingSeen;
            std::optional<bool> reloadedDuringBounce;
            std::string refusal;

            AudioExporter(*engine, bounceConfig).bounce([&](double) {
                if (!renderingSeen.has_value()) {
                    renderingSeen = scheduler->isOfflineRendering();
                    reloadedDuringBounce = store->reloadFromDisk([&](std::string error) { refusal = error; });
                }
                return true;
            });

            THEN("the render was flagged and the reload refused, leaving the change pending") {
                REQUIRE(renderingSeen == true);
                REQUIRE(reloadedDuringBounce == false);
                REQUIRE(refusal == "a render is in progress");
                REQUIRE(engine->getAudioTrackContainer()->getMasterGain() == Catch::Approx(1.0));
                REQUIRE_FALSE(engine->getUndoManager()->canUndo());

                // the stamps were left alone, so the first poll after the
                // render still sees the change - deferred, not dropped
                REQUIRE(store->projectChangedOnDisk());
                REQUIRE_FALSE(scheduler->isOfflineRendering());

                AND_THEN("the reload goes through once the render is over") {
                    REQUIRE(store->reloadFromDisk(nullptr));
                    REQUIRE(engine->getAudioTrackContainer()->getMasterGain() == Catch::Approx(0.5));
                    REQUIRE_FALSE(store->projectChangedOnDisk());
                }
            }
        }

        WHEN("the app has flagged a render before its worker started") {
            // the app takes the guard on the message thread ahead of the
            // worker, which then takes its own around the bounce
            auto render = std::make_unique<PlayListScheduler::ScopedOfflineRender>(*scheduler);

            THEN("the guards nest, and applying a state waits as well") {
                REQUIRE(scheduler->isOfflineRendering());
                {
                    const PlayListScheduler::ScopedOfflineRender nested(*scheduler);
                    REQUIRE(scheduler->isOfflineRendering());
                }
                REQUIRE(scheduler->isOfflineRendering());

                json afterState;
                engine->getProjectSerializer()->writeToJson(afterState);
                afterState["audium"]["master_gain"] = 0.25;

                REQUIRE_FALSE(store->applyStateAsUndoableReload(afterState, true, true, "Agent: test", nullptr));
                REQUIRE(engine->getAudioTrackContainer()->getMasterGain() == Catch::Approx(1.0));
                REQUIRE_FALSE(engine->getUndoManager()->canUndo());

                render.reset();
                REQUIRE_FALSE(scheduler->isOfflineRendering());

                REQUIRE(store->applyStateAsUndoableReload(afterState, true, true, "Agent: test", nullptr));
                REQUIRE(engine->getAudioTrackContainer()->getMasterGain() == Catch::Approx(0.25));
            }
        }
    }

    // cleanup ... comment out in case you need to isolate an issue
    outProject.getParentDirectory().deleteRecursively();
    bounceConfig->fileName.deleteFile();
    inputFile.deleteFile();

    engine = nullptr;
    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}
