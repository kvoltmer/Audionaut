#include <catch2/catch_test_macros.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Factory/AudioTrackFactory.h"

using namespace audium;

SCENARIO("create new session, load audio file and save", "[engine][load][save]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());
    auto engine     = AudiumFactory::createAudiumEngine();
    
    auto testFilesDirectory = String("../../../TestFiles/");
    auto inFile = File(testFilesDirectory + "silence-fade.aiff");
    
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
    
    auto testFilesDirectory = String("../../../TestFiles/Sessions/");
    auto inProject = File(testFilesDirectory + "120-funk-export.audium");
    auto outProject = File(testFilesDirectory + "legacy-test.audium/" + AudiumEngine::projectFileName);
    
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

SCENARIO("load project and save", "[engine][load][save][new]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());
    auto engine = AudiumFactory::createAudiumEngine();
    
    auto testFilesDirectory = String("../../../TestFiles/Sessions/");
    auto inProject = File(testFilesDirectory + "simple-sine.audium/Project.json");
    auto outProject = File(testFilesDirectory + "out-test.audium/" + AudiumEngine::projectFileName);
    
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

