#include <catch2/catch_test_macros.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/AudioResourceContainer.h"
#include "Engine/Factory/AudioGroupFactory.h"

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
        auto regionContainer  = engine->getAudioRegionContainer();
        int numResources = 10;
        
        WHEN("adding audio resources without any group provided")
        {
            auto group = AudioGroupFactory::createAudioGroup(*resourceContainer, *regionContainer);
            auto subGroup = group->createNewAudioSubGroup(*resourceContainer, *regionContainer);
            for (int i = 0; i < numResources; i++)
            {
                resourceContainer->addAudioResource(juce::URL(inFile), *engine, group, subGroup);
            }
            
            THEN("there must be 1 group that contains all resources")
            {
                auto groups = resourceContainer->getAudioGroups();
                REQUIRE( groups.size() == 1 );
                
                auto resources = resourceContainer->getAudioResourcesForGroup(groups[0].get());
                REQUIRE( (int)resources.size() == numResources );
            }
        }
        
        
        resourceContainer->cleanup();
        REQUIRE( resourceContainer->getNumAudioResources() == 0 );
        
        WHEN("adding 2 groups with numResources each")
        {
            
            auto group1 = AudioGroupFactory::createAudioGroup(*resourceContainer, *regionContainer);
            auto subGroup1 = group1->createNewAudioSubGroup();
            for (int i = 0; i < numResources; i++)
                resourceContainer->addAudioResource(juce::URL(inFile), *engine, group1, subGroup1);
            
            auto group2 = AudioGroupFactory::createAudioGroup(*resourceContainer, *regionContainer);
            auto subGroup2 = group2->createNewAudioSubGroup();
            for (int i = 0; i < numResources; i++)
                resourceContainer->addAudioResource(juce::URL(inFile), *engine, group2, subGroup2);
            
            THEN("we get 2 groups ")
            {
                auto groups = resourceContainer->getAudioGroups();
                REQUIRE(groups.size() == 2);
                
                for (auto group : groups)
                {
                    auto resources = resourceContainer->getAudioResourcesForGroup(group.get());
                    REQUIRE( (int)resources.size() == numResources );
                }
                
                REQUIRE( resourceContainer->getNumAudioResources() == (numResources * 2) );
            }
            
            
//            resourceContainer->deleteAudioGroup(0);
//            
//            THEN("group2 10 left")
//            {
//                auto groups = resourceContainer->getAudioGroups();
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


