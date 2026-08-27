#include <catch2/catch_test_macros.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Factory/AudioTrackFactory.h"

using namespace audium;

// std::shared_ptr<PlayListItem> PlayListContainer::createPlayListItemUI(std::shared_ptr<AudioRegion> region, int insertIndex)


SCENARIO("PlayList", "[engine][playlist]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());
    auto engine     = AudiumFactory::createAudiumEngine();
    auto store = engine->getProjectFileStore();
    
    auto testFilesDirectory = String("../../../TestFiles/");
    auto inFile = File(testFilesDirectory + "silence-fade.aiff");
    
    
    GIVEN("new project") {
        
        store->open(inFile, nullptr);
        
        WHEN("") {
            
            // TODO: testing .... argh....


            THEN("") {

            }
        }
    }
    
    
    engine = nullptr;
    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}




