#include <catch2/catch_test_macros.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Factory/AudioTrackFactory.h"
#include "Engine/Resource/ChannelMapping.h"
#include "Engine/AudioSources/AudiumTransportSource.h"
#include "Engine/Export/AudioExportThread.h"
#include "Engine/PlayList/PlayListScheduler.h"

SCENARIO("region split/create scenario", "[engine][region]")
{
    juce::MessageManager::getInstance();
    juce::MessageManagerLock mmLock(Thread::getCurrentThread());

    GIVEN("Load session file")
    {
        auto engine     = AudiumFactory::createAudiumEngine();
        engine->openFile(juce::File("../../../TestFiles/Sessions/120-funk-export-5-seconds.audium"), nullptr);

        WHEN("split at selected range")
        {
            auto selection = juce::Range<double>(2.0, 3.0);
            engine->getAudioTrackContainer()->getAudioRegionAdapter().setSelectedRange(selection, audium::seconds);
            engine->getAudioTrackContainer()->getAudioRegionAdapter().splitRegionsFromSelection(false);

            THEN("3 new regions have been created")
            {
                auto regions = engine->getAudioTrackContainer()->getAudioRegionAdapter().getAudioRegions();
                
                REQUIRE (regions[1]->getName() == "120-funk-export-01");
                REQUIRE (regions[2]->getName() == "120-funk-export-02");
                REQUIRE (regions[3]->getName() == "120-funk-export-03");

                auto playList = engine->getAudioTrackContainer()->getAudioTracks()[0]->getPlayListContainer();
                auto items = playList->getPlayListItems();
                
                REQUIRE (items[0]->getRegion()->getName() == "120-funk-export-01");
                REQUIRE (items[1]->getRegion()->getName() == "120-funk-export-02");
                REQUIRE (items[2]->getRegion()->getName() == "120-funk-export-03");
                
                REQUIRE (items[0]->getAbsolutePosition(audium::clocks) == 48.0);
                REQUIRE (items[1]->getAbsolutePosition(audium::clocks) == 96.0);
                REQUIRE (items[2]->getAbsolutePosition(audium::clocks) == 144.0);
            }
            
            
        }
        
//        engine->saveFile(juce::File("../../../TestFiles/Sessions/120-funk-export-5-seconds-out.audium"));

        engine = nullptr;
    }

    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}


