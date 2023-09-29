#include <catch2/catch_test_macros.hpp>

#include "Engine/AudiumFactory.h"
#include "Engine/AudioResourceContainer.h"

SCENARIO("resource container scenario", "[engine][resource][container]")
{
    juce::MessageManager::getInstance();
    juce::MessageManagerLock mmLock(Thread::getCurrentThread());
    auto engine     = AudiumFactory::createAudiumEngine();
    
    auto testFilesDirectory = std::string("../../../TestFiles/");
    auto inFile = File(testFilesDirectory + "silence-fade.aiff");
    
    GIVEN("the AudioResourceContainer")
    {
        auto container  = engine->getAudioResourceContainer();
        int numResources = 10;
        
        WHEN("adding audio resources without any group provided")
        {
            
            for (int i = 0; i < numResources; i++)
            {
                container->addAudioResource(juce::URL(inFile));
            }
            
            THEN("the must be 1 group that contains all resources")
            {
                auto groups = container->getAudioResourceGroups();
                REQUIRE( groups.size() == 1 );
                
                auto resources = container->getAudioResourcesForGroup(groups[0]);
                REQUIRE( (int)resources.size() == numResources );
            }
        }
        
        
        container->cleanup();
        REQUIRE( container->getNumAudioResources() == 0 );
        
        WHEN("adding 2 groups with numResources each")
        {
            
            auto group1 = std::shared_ptr<AudioResourceGroup> (new AudioResourceGroup(*container, "Group 1"));
            for (int i = 0; i < numResources; i++)
                container->addAudioResource(juce::URL(inFile), group1);
            
            auto group2 = std::shared_ptr<AudioResourceGroup> (new AudioResourceGroup(*container, "Group 2"));
            for (int i = 0; i < numResources; i++)
                container->addAudioResource(juce::URL(inFile), group2);
            
            THEN("we get 2 groups ")
            {
                auto groups = container->getAudioResourceGroups();
                REQUIRE(groups.size() == 2);
                
                for (auto group : groups)
                {
                    auto resources = container->getAudioResourcesForGroup(group);
                    REQUIRE( (int)resources.size() == numResources );
                }
                
                REQUIRE( container->getNumAudioResources() == (numResources * 2) );
            }
        }
        
    }
    
    engine = nullptr;
    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}


