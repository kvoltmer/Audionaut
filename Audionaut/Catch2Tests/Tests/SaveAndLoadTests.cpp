#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/ProjectFileStore.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Factory/AudioTrackFactory.h"

using namespace audium;

auto testFilesDirectory = String(CURRENT_SOURCE_DIR) + String("/TestFiles/");

SCENARIO("create new session, load audio file and save", "[engine][load][save]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());
    auto engine     = AudiumFactory::createAudiumEngine();
        
    auto inFile = File(testFilesDirectory + "silence-fade.aiff");
    REQUIRE(inFile.existsAsFile());
    
    auto outProjectFile = File(testFilesDirectory + "Sessions/testing.audium/" + ProjectFileStore::projectFileName);
    
    GIVEN("new project") {
        engine->createNewProject();
        
        engine->openFile(inFile, nullptr);
        
        WHEN("save project") {
            REQUIRE(engine->saveFile(outProjectFile, nullptr));

            THEN("examine project") {
                REQUIRE(outProjectFile.exists());
                
                auto audioFileDir = AudioResourceContainer::getAudioFileDirectory();
                REQUIRE(audioFileDir.exists());
                
                auto audioFile = File(audioFileDir.getFullPathName() + File::getSeparatorString() + inFile.getFileName());
                REQUIRE(audioFile.existsAsFile());
            }
        }
    }
    
    // cleanup ... comment out in case you need to isolate an issue
    outProjectFile.getParentDirectory().deleteRecursively();
    
    engine = nullptr;
    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}


SCENARIO("load legacy project and save", "[engine][load][save][leagacy]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());
    auto engine = AudiumFactory::createAudiumEngine();
    
    
    auto inProject = File(testFilesDirectory + "/Sessions/120-funk-export.audium");
    REQUIRE(inProject.existsAsFile());
    auto outProject = File(testFilesDirectory + "/Sessions/legacy-test.audium/" + ProjectFileStore::projectFileName);
    
    GIVEN("open legacy project") {
    
        engine->openFile(inProject, nullptr);
        
        WHEN("save project") {
            REQUIRE(engine->saveFile(outProject, nullptr));

            THEN("examine project") {
                REQUIRE(outProject.exists());
                
                auto audioFileDir = AudioResourceContainer::getAudioFileDirectory();
                REQUIRE(audioFileDir.exists());
            }
        }
    }
    // cleanup ... comment out in case you need to isolate an issue
    outProject.getParentDirectory().deleteRecursively();
    
    engine = nullptr;
    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("view state is persisted with the project", "[engine][load][save][ui_state]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());
    auto engine = AudiumFactory::createAudiumEngine();

    auto outProject = File(testFilesDirectory + "/Sessions/ui-state-test.audium/" + ProjectFileStore::projectFileName);

    GIVEN("a project with a zoom factor and a scroll position") {
        engine->createNewProject();

        engine->getUiState()["arrangement"]["zoom_factor"] = 4.0;
        engine->getUiState()["arrangement"]["scroll_x"] = 1234.5;
        engine->getUiState()["arrangement"]["scroll_y"] = 42.0;

        WHEN("the project is saved and loaded again") {
            REQUIRE(engine->saveFile(outProject, nullptr));
            REQUIRE(engine->openFile(outProject.getParentDirectory(), nullptr));

            THEN("the view state is recalled") {
                auto& arrangement = engine->getUiState()["arrangement"];
                REQUIRE(arrangement["zoom_factor"].template get<double>() == 4.0);
                REQUIRE(arrangement["scroll_x"].template get<double>() == 1234.5);
                REQUIRE(arrangement["scroll_y"].template get<double>() == 42.0);
            }
        }

        WHEN("a new project is created") {
            engine->cleanup();
            engine->createNewProject();

            THEN("the view state does not carry over") {
                REQUIRE_FALSE(engine->getUiState().contains("arrangement"));
            }
        }
    }
    // cleanup ... comment out in case you need to isolate an issue
    outProject.getParentDirectory().deleteRecursively();

    engine = nullptr;
    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("load project and save", "[engine][load][save][new]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());
    auto engine = AudiumFactory::createAudiumEngine();
    
    
    auto inProject = File(testFilesDirectory + "/Sessions/simple-sine.audium/Project.json");
    auto outProject = File(testFilesDirectory + "/Sessions/out-test.audium/" + ProjectFileStore::projectFileName);
    
    GIVEN("open project") {

        engine->openFile(inProject, nullptr);

        WHEN("save project") {
            REQUIRE(engine->saveFile(outProject, nullptr));

            THEN("examine project") {
                REQUIRE(outProject.exists());

                auto audioFileDir = AudioResourceContainer::getAudioFileDirectory();
                REQUIRE(audioFileDir.exists());
            }
        }
    }
    // cleanup ... comment out in case you need to isolate an issue
    outProject.getParentDirectory().deleteRecursively();

    engine = nullptr;
    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("clip gain migrates from legacy region gain and persists", "[engine][load][save][clip][volume]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());
    auto engine = AudiumFactory::createAudiumEngine();

    auto sourceSession = File(testFilesDirectory + "Sessions/move-channels.audium");
    REQUIRE(sourceSession.exists());

    // open a disposable copy, never the checked-in session (see MoveChannelsTests)
    auto fileUnderTest = File::getSpecialLocation(File::tempDirectory)
                             .getChildFile("clip-gain-migration.audium")
                             .getNonexistentSibling();
    REQUIRE(sourceSession.copyDirectoryTo(fileUnderTest));

    auto outProject = File(testFilesDirectory + "Sessions/clip-gain-test.audium/" + ProjectFileStore::projectFileName);

    GIVEN("a legacy project with gains stored on the regions") {
        REQUIRE(engine->openFile(fileUnderTest, nullptr));

        auto playList = engine->getAudioTrackContainer()->getAudioTrack(0)->getPlayListContainer();

        // migrated into the playlist items on load
        REQUIRE(playList->getPlayListItem(1)->getDynamics().getGain(0) == Catch::Approx(1.4125375446227544));
        REQUIRE(playList->getPlayListItem(1)->getDynamics().getGain(1) == Catch::Approx(0.7752542528795534));
        REQUIRE(playList->getPlayListItem(2)->getDynamics().getGain(0) == Catch::Approx(1.9952623149688797));
        REQUIRE(playList->getPlayListItem(2)->getDynamics().getGain(1) == Catch::Approx(0.25118864315095796));

        WHEN("a gain is changed and the project is saved and loaded again") {
            playList->getPlayListItem(0)->getDynamics().setGain(0, 0.25);

            REQUIRE(engine->saveFile(outProject, nullptr));
            REQUIRE(engine->openFile(outProject.getParentDirectory(), nullptr));

            THEN("the new and the migrated gains survive the round trip") {
                auto reloaded = engine->getAudioTrackContainer()->getAudioTrack(0)->getPlayListContainer();
                REQUIRE(reloaded->getPlayListItem(0)->getDynamics().getGain(0) == Catch::Approx(0.25));
                REQUIRE(reloaded->getPlayListItem(1)->getDynamics().getGain(0) == Catch::Approx(1.4125375446227544));
                REQUIRE(reloaded->getPlayListItem(1)->getDynamics().getGain(1) == Catch::Approx(0.7752542528795534));
                REQUIRE(reloaded->getPlayListItem(2)->getDynamics().getGain(0) == Catch::Approx(1.9952623149688797));
                REQUIRE(reloaded->getPlayListItem(2)->getDynamics().getGain(1) == Catch::Approx(0.25118864315095796));
            }
        }
    }
    // cleanup ... comment out in case you need to isolate an issue
    outProject.getParentDirectory().deleteRecursively();
    fileUnderTest.deleteRecursively();

    engine = nullptr;
    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

