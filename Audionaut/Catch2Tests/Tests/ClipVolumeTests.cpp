#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Factory/AudioTrackFactory.h"
#include "Engine/Resource/ChannelMapping.h"
#include "Engine/AudioSources/AudiumTransportSource.h"
#include "Engine/Export/AudioExporter.h"
#include "Engine/PlayList/PlayListScheduler.h"

#include "Engine/PlayList/TransportLoop.h"
#include "Engine/Provider/TempoProvider.h"
#include "Engine/Recording/RecordingActionHandler.h"

#include "TestUtils.h"

using namespace audium;
using namespace juce;


SCENARIO("recording scenario", "[engine][clip][volume]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());
    
    
    
    auto sr = 44100.0;
    // generate DC offset
    auto inputFile = generateDcOffsetAudioFile(2.0);
    //auto inBuffer = audioFileToAudioBuffer(inputFile);
    
    auto bounceConfig = std::make_shared<audium::ExportAudioConfig>();
    bounceConfig->fileName = File(String(CURRENT_SOURCE_DIR) + String("/TestFiles/rec-out.wav"));
    bounceConfig->sampleRate = sr;
    bounceConfig->blockSize = 1024 * 4;
    bounceConfig->numChannels = 1;

    auto engine = AudiumFactory::createAudiumEngine();

    GIVEN("generated audio file with loop")
    {
        engine->openFile(inputFile, nullptr);
        engine->getAudioTrackContainer()->getAudioRegionAdapter().splitRegions(1.0, audium::seconds);
        
        auto track = engine->getAudioTrackContainer()->getAudioTrack(0);
        REQUIRE(track->getPlayListContainer()->getNumItems() == 2);
        REQUIRE(track->getPlayListContainer()->sortedByPosition());
        
        // set gain 0.5 for second region
        track->getPlayListContainer()->getPlayListItem(1)->getDynamics().setGain(0, 0.5);
        
        engine->getPlayListScheduler()->commitPlayListData();
        
        bounceConfig->lengthSeconds = engine->getPlayListScheduler()->getTotalLength(audium::seconds);
        
        REQUIRE(2.0 == Catch::Approx(bounceConfig->lengthSeconds));
        
        WHEN("bounce audio")
        {
            auto exporter = std::make_unique<AudioExporter>(*engine, bounceConfig);
            exporter->bounce();
            
            THEN("examine bounced audio file")
            {
                auto buffer = audioFileToAudioBuffer(bounceConfig->fileName);
                auto samplePerPhase = 44100;
                
                // check second 1
                for (auto s = 0; s < samplePerPhase; s++) {
                    auto val1 = buffer.getSample(0, s);
                    auto margin = 0.000001f;
                    REQUIRE(1.f == Catch::Approx(val1).margin(margin));
                }

                // check second 2
                for (auto s = samplePerPhase; s < samplePerPhase * 2; s++) {
                    auto val1 = buffer.getSample(0, s);
                    auto margin = 0.000001f;
                    REQUIRE(0.5f == Catch::Approx(val1).margin(margin));
                }
            }
        }
    }

    engine = nullptr;
    
    if (bounceConfig->fileName.existsAsFile())
        bounceConfig->fileName.deleteFile();
    
    if (inputFile.existsAsFile())
        inputFile.deleteFile();


    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}

SCENARIO("clip gain is per playlist item, not per region", "[engine][clip][volume]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    auto inputFile = generateDcOffsetAudioFile(2.0);
    auto engine = AudiumFactory::createAudiumEngine();

    GIVEN("two playlist items sharing one region")
    {
        engine->openFile(inputFile, nullptr);

        auto track = engine->getAudioTrackContainer()->getAudioTrack(0);
        auto item0 = track->getPlayListContainer()->getPlayListItem(0);
        REQUIRE(item0 != nullptr);

        auto region = item0->getRegion();
        auto item1 = track->getPlayListContainer()->createPlayListItemAtPositionUI(region, 2.0, audium::seconds);
        REQUIRE(item1 != nullptr);
        REQUIRE(item1->getRegion() == region);

        WHEN("the gain of the first item is changed")
        {
            item0->getDynamics().setGain(0, 0.5);

            THEN("the second item keeps its own gain")
            {
                REQUIRE(item0->getDynamics().getGain(0) == Catch::Approx(0.5));
                REQUIRE(item1->getDynamics().getGain(0) == Catch::Approx(1.0));
            }
        }
    }

    engine = nullptr;

    if (inputFile.existsAsFile())
        inputFile.deleteFile();

    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}

