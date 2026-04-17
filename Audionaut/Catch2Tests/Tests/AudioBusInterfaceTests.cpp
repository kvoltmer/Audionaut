#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Core/LockFreeContainer.h"

using namespace audium;

SCENARIO("audiobus interface scenario", "[engine][audiobus]")
{
    GIVEN("the audio bus interface")
    {
        auto audioBusRenderer   = std::make_shared<AudioBusRenderer<float>>(nullptr, nullptr);
        auto lockFreeCommander  = std::make_shared<LockFreeCommander>(256);
        auto audioBusInterface  = std::make_shared<AudioBusInterface>(lockFreeCommander, audioBusRenderer);
        audioBusRenderer->setNumAudioBusChannels(4);
        
        WHEN("setting mute and solo for channel 0")
        {
            AudioChannelData d;
            d.mute = true;
            d.solo = true; // solo overrides mute
            audioBusInterface->setChannelData(0, d);
            REQUIRE(audioBusInterface->getChannelData(0).mute);
            REQUIRE(audioBusInterface->getChannelData(0).solo);
                        
            THEN("cannel 0 gain is 1, all other channels are quiet")
            {
                REQUIRE(Catch::Approx(1.f) == audioBusRenderer->getGain(0).getGainLinear());
                REQUIRE(Catch::Approx(0.f) == audioBusRenderer->getGain(1).getGainLinear());
                REQUIRE(Catch::Approx(0.f) == audioBusRenderer->getGain(2).getGainLinear());
                REQUIRE(Catch::Approx(0.f) == audioBusRenderer->getGain(3).getGainLinear());
            }
        }

        WHEN("setting chan 0 and 1 to solo and un-solo chan 0")
        {
            AudioChannelData d;
            d.solo = true;
            audioBusInterface->setChannelData(0, d);
            audioBusInterface->setChannelData(1, d);
            
            // set solo to false for channel 0
            AudioChannelData d2;
            audioBusInterface->setChannelData(0, d2);
                        
            THEN("channel 1 is still solo")
            {
                REQUIRE(Catch::Approx(0.f) == audioBusRenderer->getGain(0).getGainLinear());
                REQUIRE(Catch::Approx(1.f) == audioBusRenderer->getGain(1).getGainLinear());
                REQUIRE(Catch::Approx(0.f) == audioBusRenderer->getGain(2).getGainLinear());
                REQUIRE(Catch::Approx(0.f) == audioBusRenderer->getGain(3).getGainLinear());
            }
        }
        
        
    }
}


