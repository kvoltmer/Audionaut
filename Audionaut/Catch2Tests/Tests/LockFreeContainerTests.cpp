#include <catch2/catch_test_macros.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Factory/AudioTrackFactory.h"

#include "Engine/Core/LockFreeContainer.h"
#include "Engine/Core/DspClipData.h"

using namespace audium;

SCENARIO("lock free container scenario", "[engine][lock-free][container]")
{
    GIVEN("a LockFreeContainer")
    {
        auto container = std::make_unique<audium::LockFreeContainer<DspClipData>>(1024);
        
        // test data
        std::size_t numItems = 100;
        std::vector<DspClipData> test_data;
        for (std::size_t i = 0; i < numItems; i++) {
            DspClipData clip;
            clip.clipData.regionData.setStart(i);
            clip.clipData.regionData.setEnd(i+1);
            test_data.push_back(clip);
        }
        
        WHEN("adding number of items")
        {
            for (auto clip : test_data) {
                container->getProducerObjects().push_back(clip);
            }
            container->commit();
            
            THEN("number of items must exist")
            {
                REQUIRE(container->pull());
                auto lockFree = container->getConsumerObjects();
                
                REQUIRE(lockFree.size() == numItems);
                auto counter = 0;
                for (auto clip : lockFree) {
                    REQUIRE((int)clip.clipData.regionData.getStart() == counter);
                    REQUIRE((int)clip.clipData.regionData.getEnd() == counter+1);
                    counter++;
                }
                
                container->clear();
                container->commit();
                
                REQUIRE(container->pull());
                lockFree = container->getConsumerObjects();
                REQUIRE(lockFree.size() == 0);
            }
        }
    }
}


