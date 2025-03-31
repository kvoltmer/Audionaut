#include <catch2/catch_test_macros.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Factory/AudioTrackFactory.h"
#include "Engine/Resource/ChannelMapping.h"

using namespace audium;

SCENARIO("channel mappping scenario", "[engine][channel][mapping]")
{
    GIVEN("channel mapping")
    {
        auto mapping = std::make_unique<audium::ChannelMapping>();
        
        WHEN("mapping source 7 to destination 3")
        {
            mapping->setOutputChannelMapping(7, 3);
            
            THEN("source 7 is mapped to destination 3")
            {
                REQUIRE(mapping->getSourceChannel() == 7);
                REQUIRE(mapping->getDestinationChannel() == 3);
            }
        }
    }
}


