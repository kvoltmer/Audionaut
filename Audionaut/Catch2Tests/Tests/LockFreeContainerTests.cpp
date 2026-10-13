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

SCENARIO("lock free container hands over whole snapshots", "[engine][lock-free][container]")
{
    auto makeClip = [] (int start)
    {
        DspClipData clip;
        clip.clipData.regionData.setStart(start);
        clip.clipData.regionData.setEnd(start + 1);
        return clip;
    };

    GIVEN("a container with nothing committed")
    {
        audium::LockFreeContainer<DspClipData> container(16);

        THEN("pull reports nothing new, and the consumer view is empty")
        {
            REQUIRE_FALSE(container.pull());
            REQUIRE(container.getConsumerObjects().empty());
        }

        WHEN("two snapshots are committed before the consumer pulls")
        {
            container.getProducerObjects().push_back(makeClip(1));
            container.commit();

            container.clear();
            container.getProducerObjects().push_back(makeClip(2));
            container.getProducerObjects().push_back(makeClip(3));
            container.commit();

            THEN("one pull delivers the latest snapshot whole, and the next pull has nothing")
            {
                REQUIRE(container.pull());
                const auto& clips = container.getConsumerObjects();
                REQUIRE(clips.size() == 2);
                REQUIRE((int) clips[0].clipData.regionData.getStart() == 2);
                REQUIRE((int) clips[1].clipData.regionData.getStart() == 3);

                REQUIRE_FALSE(container.pull());
                REQUIRE(container.getConsumerObjects().size() == 2);
            }
        }

        WHEN("more objects than the reserved capacity are committed")
        {
            for (auto i = 0; i < 1000; ++i)
                container.getProducerObjects().push_back(makeClip(i));
            container.commit();

            THEN("none are dropped")
            {
                REQUIRE(container.pull());
                REQUIRE(container.getConsumerObjects().size() == 1000);
                REQUIRE((int) container.getConsumerObjects().back().clipData.regionData.getStart() == 999);
            }
        }

        WHEN("commits and pulls alternate many times")
        {
            for (auto round = 0; round < 50; ++round)
            {
                container.clear();
                container.getProducerObjects().push_back(makeClip(round));
                container.commit();

                if (round % 3 != 2)   // every third snapshot is skipped by the consumer
                {
                    REQUIRE(container.pull());
                    REQUIRE((int) container.getConsumerObjects()[0].clipData.regionData.getStart() == round);
                }
            }

            THEN("the consumer still sees the last snapshot it pulled")
            {
                REQUIRE((int) container.getConsumerObjects()[0].clipData.regionData.getStart() == 49);
            }
        }
    }
}
