#include <catch2/catch_test_macros.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Factory/AudioTrackFactory.h"

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
        
        WHEN("adding audio resources without any group provided")
        {
            auto group = AudioTrackFactory::createAudioTrack(*groupContainer, resourceContainer);
            auto subGroup = group->createNewAudioSubGroup(0.0, audium::clocks);
            for (int i = 0; i < numResources; i++)
            {
                resourceContainer->addAudioResource(juce::URL(inFile), group, subGroup, 0);
            }
            
            THEN("there must be 1 group that contains all resources")
            {
                auto groups = resourceContainer->getAudioTracks();
                //REQUIRE( groups.size() == 1 );
                
                //auto resources = resourceContainer->getAudioResourcesForGroup(groups[0].get());
                //REQUIRE( (int)resources.size() == numResources );
            }
        }
        
        
        resourceContainer->cleanup();
        REQUIRE( resourceContainer->getNumAudioResources() == 0 );
        
        WHEN("adding 2 groups with numResources each")
        {
            
//            auto group1 = AudioTrackFactory::createAudioTrack(*resourceContainer, *regionContainer);
//            auto subGroup1 = group1->createNewAudioSubGroup();
//            for (int i = 0; i < numResources; i++)
//                resourceContainer->addAudioResource(juce::URL(inFile), *engine, group1, subGroup1);
//            
//            auto group2 = AudioTrackFactory::createAudioTrack(*resourceContainer, *regionContainer);
//            auto subGroup2 = group2->createNewAudioSubGroup();
//            for (int i = 0; i < numResources; i++)
//                resourceContainer->addAudioResource(juce::URL(inFile), *engine, group2, subGroup2);
//            
//            THEN("we get 2 groups ")
//            {
//                auto groups = resourceContainer->getAudioTracks();
//                REQUIRE(groups.size() == 2);
//                
//                for (auto group : groups)
//                {
//                    auto resources = resourceContainer->getAudioResourcesForGroup(group.get());
//                    REQUIRE( (int)resources.size() == numResources );
//                }
//                
//                REQUIRE( resourceContainer->getNumAudioResources() == (numResources * 2) );
//            }
//            
//            
//            resourceContainer->deleteAudioTrack(0);
//            
//            THEN("group2 10 left")
//            {
//                auto groups = resourceContainer->getAudioTracks();
//                REQUIRE(groups.size() == 1);
//                
//                REQUIRE( resourceContainer->getNumAudioResources() == numResources );
//            }
        }
        
    }
    
    engine = nullptr;
    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}


