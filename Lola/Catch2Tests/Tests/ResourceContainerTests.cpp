#include <catch2/catch_test_macros.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Factory/AudioTrackFactory.h"

using namespace audium;

SCENARIO("resource container scenario", "[engine][resource][container]")
{
    juce::MessageManager::getInstance();
    juce::MessageManagerLock mmLock(Thread::getCurrentThread());
    auto engine     = AudiumFactory::createAudiumEngine();
    
    auto testFilesDirectory = std::string("../../../TestFiles/");
    auto inFile = File(testFilesDirectory + "silence-fade.aiff");
    
    GIVEN("the AudioResourceContainer")
    {
        auto resourceContainer  = engine->getAudioResourceContainer();
        auto groupContainer  = engine->getAudioTrackContainer();
        int numResources = 10;
        
        WHEN("adding audio resources without any track provided")
        {
//            auto group = AudioTrackFactory::createAudioTrack(*groupContainer, resourceContainer);
//            auto subGroup = group->createNewAudioSubGroup(0.0, audium::clocks);
//            for (int i = 0; i < numResources; i++)
//            {
//                resourceContainer->addAudioResource(juce::URL(inFile), group, subGroup, 0);
//            }
//
//            THEN("there must be 1 group that contains all resources")
//            {
//                auto groups = resourceContainer->getAudioTracks();
//                //REQUIRE( groups.size() == 1 );
//
//                //auto resources = resourceContainer->getAudioResourcesForGroup(groups[0].get());
//                //REQUIRE( (int)resources.size() == numResources );
//            }
        }
        
        
        resourceContainer->cleanup();
        REQUIRE( resourceContainer->getNumAudioResources() == 0 );
        

    }
    
    engine = nullptr;
    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}


