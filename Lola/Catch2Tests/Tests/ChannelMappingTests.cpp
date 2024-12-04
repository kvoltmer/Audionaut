#include <catch2/catch_test_macros.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Factory/AudioTrackFactory.h"
#include "Engine/Resource/ChannelMapping.h"

SCENARIO("channel mappping scenario", "[engine][channel][mapping]")
{
    GIVEN("channel mapping")
    {
        auto mapping = std::make_unique<audium::ChannelMapping>();
        
        WHEN("mapping source 2 and 7 to destination 3")
        {
            mapping->setOutputChannelMapping(2, 3);
            mapping->setOutputChannelMapping(7, 3);
            
            THEN("source 7 is mapped to destination 3")
            {
                REQUIRE(mapping->getRemappedChannel(2) == -1);
                REQUIRE(mapping->getRemappedChannel(7) == 3);
                REQUIRE(mapping->getSourceChannel(3) == 7);
            }
        }
    }
    
    
    GIVEN("8 channel mapping")
    {
        auto mapping = std::make_unique<audium::ChannelMapping>();
        for (auto i = 0; i < 8; i++)
        {
            mapping->setOutputChannelMapping(i, i);
        }
        
        WHEN("deleting channel")
        {
            mapping->deleteChannel(0);
            mapping->decrementChannelMapping(0);
            
            REQUIRE(mapping->getRemappedChannel(0) == -1);
            REQUIRE(mapping->getRemappedChannel(1) == 0);
            
//            for (auto i = 0; i < 8; i++)
//                std::cout << i << " " << mapping->getRemappedChannel(i) << std::endl;
            
            mapping->deleteChannel(0);
            mapping->decrementChannelMapping(0);
            
//            for (auto i = 0; i < 8; i++)
//                std::cout << i << " " << mapping->getRemappedChannel(i) << std::endl;
            
            REQUIRE(mapping->getRemappedChannel(0) == -1);
            REQUIRE(mapping->getRemappedChannel(1) == -1);
            REQUIRE(mapping->getRemappedChannel(2) == 0);
            

        }
    }
    
}


