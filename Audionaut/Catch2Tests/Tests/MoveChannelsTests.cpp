#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Factory/AudioTrackFactory.h"
#include "Engine/Channel/AudioChannel.h"

using namespace audium;

SCENARIO("move channels scenario", "[engine][channels]")
{
    juce::MessageManager::getInstance();
    juce::MessageManagerLock mmLock(Thread::getCurrentThread());

    GIVEN("Load session file")
    {
        auto engine     = AudiumFactory::createAudiumEngine();
        auto fileUnderTest = File(String(CURRENT_SOURCE_DIR) + String("/TestFiles/Sessions/move-channels.audium"));
        REQUIRE(fileUnderTest.exists());
        auto ok = engine->openFile(fileUnderTest, nullptr);
        REQUIRE(ok);
        
        const int numChannels = 2;
        // gain values for the test (2 channels, 3 gain values)
        double gains[numChannels][3] = {
            { 0.0, 0.0, 0.0 },
            { 0.0, 0.0, 0.0 }
        };
        
        WHEN("copy selected channels to new track")
        {
            auto channelNumber = 1;
            auto track = engine->getAudioTrackContainer()->getAudioTrack(0);
            REQUIRE(track != nullptr);
            REQUIRE(track->getNumAudioTrackChannels() == numChannels);
            
            for (int i = 0; i < numChannels; ++i) {
                gains[i][0] = track->getPlayListContainer()->getPlayListItem(0)->getRegion()->getGain(i);
                gains[i][1] = track->getPlayListContainer()->getPlayListItem(1)->getRegion()->getGain(i);
                gains[i][2] = track->getPlayListContainer()->getPlayListItem(2)->getRegion()->getGain(i);
                
                // select all channels
                track->audioChannelContainer->objects[(std::size_t)i]->setSelected(true);
            }
            
            
            // copy selected channels to new track channel 0
            engine->getAudioTrackContainer()->copySelectedChannelsToNewTrack();
            
            THEN("the gain values if the new track must match")
            {
                track = engine->getAudioTrackContainer()->getAudioTrack(1);
                REQUIRE(track != nullptr);
                
                double new_gains[numChannels][3] = {
                    { 0.0, 0.0, 0.0 },
                    { 0.0, 0.0, 0.0 }
                };
                
                for (int i = 0; i < numChannels; ++i) {
                    new_gains[i][0] = track->getPlayListContainer()->getPlayListItem(0)->getRegion()->getGain(i);
                    new_gains[i][1] = track->getPlayListContainer()->getPlayListItem(1)->getRegion()->getGain(i);
                    new_gains[i][2] = track->getPlayListContainer()->getPlayListItem(2)->getRegion()->getGain(i);
                }
                
                
                // check gain values
                for (int i = 0; i < numChannels; ++i) {
                    for (int j = 0; j < 3; ++j) {
                        REQUIRE(gains[i][j] == Catch::Approx(new_gains[i][j]));
                    }
                }
            }
        }
    
        engine = nullptr;
    }

    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}


