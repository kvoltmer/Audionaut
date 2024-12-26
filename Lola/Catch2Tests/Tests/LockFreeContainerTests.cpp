#include <catch2/catch_test_macros.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Factory/AudioTrackFactory.h"

#include "Engine/Core/LockFreeContainer.h"

class Dummy {
    
};

SCENARIO("lock free container scenario", "[engine][lock-free][container]")
{
    GIVEN("a LockFreeContainer")
    {
        auto container = std::make_unique<audium::LockFreeContainer<Dummy, 1024>>();
        auto numItems = 100;
        WHEN("adding number of items")
        {
            std::vector<std::shared_ptr<Dummy>> vector;
            for (auto i = 0; i < numItems; i++) {
                auto dummy = std::make_shared<Dummy>();
                container->add(dummy);
                vector.push_back(dummy);
            }
            
            THEN("number of items must exist")
            {
                auto lockFree = container->getObjectsLockFree();
                auto count = 0;
                for (auto item : lockFree)
                {
                    if (item != nullptr)
                        count++;
                }
                REQUIRE(count == numItems);
                REQUIRE(container->getObjects().size() == numItems);
                

            }
            AND_THEN("container ist clean")
            {
                for (auto d : vector) {
                    container->remove(d);
                }
                
                auto lockFree = container->getObjectsLockFree();
                auto count = 0;
                for (auto item : lockFree)
                {
                    if (item != nullptr)
                        count++;
                }
                REQUIRE(count == 0);
                REQUIRE(container->getObjects().size() == 0);
            }
        }
    }
}


