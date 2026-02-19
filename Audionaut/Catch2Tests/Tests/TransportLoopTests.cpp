#include <catch2/catch_test_macros.hpp>

//#include "Engine/Factory/AudiumFactory.h"
//#include "Engine/Resource/AudioResourceContainer.h"
//#include "Engine/Factory/AudioTrackFactory.h"
//
//#include "Engine/Core/LockFreeContainer.h"
//#include "Engine/Core/DspClipData.h"

#include "Engine/PlayList/TransportLoop.h"
#include "Engine/Provider/TempoProvider.h"


using namespace audium;
using namespace juce;


SCENARIO("transport loop scenario", "[engine][transport][loop]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    
    GIVEN("a TransportLoop")
    {
        auto tempoProvider = std::make_shared<TempoProvider>(nullptr);
        auto transportLoop = std::make_unique<audium::TransportLoop>(nullptr,
                                                                     tempoProvider);
        transportLoop->setLoopActive(true);
        transportLoop->prepareToPlay(512, 44100.0);
        
        
        WHEN("processing the loop")
        {
            // loop between 0 and 1
            transportLoop->setLoopPositionRange(nullptr, {0.0, 1.0}, audium::clocks);
            auto externalPos = 0.0;
            for (auto i = 0; i < 101; i++) {
                
                auto loopPos = externalPos;
                transportLoop->processLoop(loopPos, 512);
                
                externalPos += 1.0;
            }
            
            THEN("check on loop count")
            {
                REQUIRE(transportLoop->getLoopCount() == 100);
            }
        }
        
        WHEN("loop count for position")
        {
            // loop between 2 and 3
            transportLoop->setLoopPositionRange(nullptr, {2.0, 3.0}, audium::clocks);
            
            THEN("check on loop count for position")
            {
                auto phase = 0.0;
                // start < loop start and process 1.5 -> end up in the middle of the loop
                phase = transportLoop->getLoopPhaseForPosition(1.0, 1.5, audium::clocks);
                REQUIRE(static_cast<int>(phase) == 0);
                // hit the loop end -> should count as 1
                phase = transportLoop->getLoopPhaseForPosition(1.0, 2.0, audium::clocks);
                REQUIRE(static_cast<int>(phase) == 1);

                phase = transportLoop->getLoopPhaseForPosition(2.5, 0.0, audium::clocks);
                REQUIRE(static_cast<int>(phase) == 0);
                
                phase = transportLoop->getLoopPhaseForPosition(3.0, 0.0, audium::clocks);
                REQUIRE(static_cast<int>(phase) == 1);
                
            }
        }
    }
    
    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}




