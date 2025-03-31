#include <catch2/catch_test_macros.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Factory/AudioTrackFactory.h"

using namespace audium;

SCENARIO("load and save scenario", "[engine][load][save]")
{
    juce::MessageManager::getInstance();
    juce::MessageManagerLock mmLock(Thread::getCurrentThread());
    auto engine     = AudiumFactory::createAudiumEngine();
    
    auto testFilesDirectory = std::string("../../../TestFiles/");
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
    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}


