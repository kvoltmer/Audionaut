#include <catch2/catch_test_macros.hpp>

#include "Engine/Factory/AudiumFactory.h"
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
    
    auto outProjectFile = File(testFilesDirectory + "Sessions/testing.audium/" + AudiumEngine::projectFileName);
    
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
    auto outProject = File(testFilesDirectory + "/Sessions/legacy-test.audium/" + AudiumEngine::projectFileName);
    
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

    auto outProject = File(testFilesDirectory + "/Sessions/ui-state-test.audium/" + AudiumEngine::projectFileName);

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
    auto outProject = File(testFilesDirectory + "/Sessions/out-test.audium/" + AudiumEngine::projectFileName);
    
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

