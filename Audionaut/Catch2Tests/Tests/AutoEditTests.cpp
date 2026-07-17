#include <catch2/catch_test_macros.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Factory/AudioTrackFactory.h"
#include "Engine/AutoEdit/AutoEdit.h"

using namespace audium;

SCENARIO("load audio file and do auto edit", "[engine][autoedit]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());
    
    
    auto engine     = AudiumFactory::createAudiumEngine();
        
    auto testFilesDirectory = String(CURRENT_SOURCE_DIR) + String("/TestFiles/");
    auto inFile = File(testFilesDirectory + "_export_TRK-18.wav");
    REQUIRE(inFile.existsAsFile());
    
//    auto outProjectFile = File(testFilesDirectory + "Sessions/testing.audium/" + AudiumEngine::projectFileName);
 //   engine->openFile(inFile, nullptr);

    engine->getAudioTrackContainer()->addAudioFiles({inFile.getFullPathName()},
                                                    0.0,
                                                    nullptr,
                                                    false);        

    GIVEN("new project") {
        //engine->createNewProject();
        
        
        auto autoEdit = std::make_unique<AutoEdit>(engine);
        AutoEditConfig config;
        config.trackId = 0;
        config.playlistItemId = 0;
        autoEdit->invokeAutoEdit(config, [](std::string error) {
            std::cout << "error: " << error << std::endl;
        });
        
        WHEN("save project") {
//            REQUIRE(engine->saveFile(outProjectFile, nullptr));

            THEN("examine project") {
//                REQUIRE(outProjectFile.exists());
//                
//                auto audioFileDir = AudioResourceContainer::getAudioFileDirectory();
//                REQUIRE(audioFileDir.exists());
//                
//                auto audioFile = File(audioFileDir.getFullPathName() + File::getSeparatorString() + inFile.getFileName());
//                REQUIRE(audioFile.existsAsFile());
            }
        }
    }
    
    engine = nullptr;
    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

