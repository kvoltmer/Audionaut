#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/ProjectFileStore.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Factory/AudioTrackFactory.h"
#include "Engine/Resource/ChannelMapping.h"
#include "Engine/AudioSources/VoiceSource.h"
#include "Engine/Export/AudioExporter.h"
#include "Engine/PlayList/PlayListScheduler.h"

#include "Engine/PlayList/TransportLoop.h"
#include "Engine/Provider/TempoProvider.h"

#include "TestUtils.h"

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
            transportLoop->setLoopPositionRange(nullptr, {0.0, 1.0}, audium::seconds);
            auto transportPos = 0.0;
            auto delta = 0.1; // seconds
            
            for (auto i = 0; i < 1001; i++) {
                
                auto samples = static_cast<int>(44100.0 * delta);
                auto result = transportLoop->processLoop(transportPos, samples, audium::clocks);
                if (result.loopEvent) {
                    REQUIRE(result.numSamplesUntilLoop >= 0);
                    REQUIRE(result.numSamplesUntilLoop <= samples);
                }
                
                
                // fake transport
                auto inc = tempoProvider->secondsToClocks(0.1);
                
                transportPos += inc;
            }
            
            THEN("check on loop count")
            {
                REQUIRE(transportLoop->getLoopCount() == 100);
            }
        }
        
        WHEN("loop phase for position")
        {
            // loop between 2 and 3
            transportLoop->setLoopPositionRange(nullptr, {2.0, 3.0}, audium::clocks);
            
            THEN("check on loop phase for position")
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

static void examineBouncedAudioFile(std::shared_ptr<audium::ExportAudioConfig> bounceConfig)
{
    AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    
    std::unique_ptr<AudioFormatReader> reader;
    reader.reset(formatManager.createReaderFor(bounceConfig->fileName));

    jassert(reader);
    
    AudioBuffer<float> buffer((int)reader->numChannels, (int)reader->lengthInSamples);
    
    auto success = reader->read(&buffer,
                                0,
                                (int)reader->lengthInSamples,
                                0,
                                true,
                                true);
    REQUIRE(success);
    
    for (auto i = 1; i < static_cast<int>(bounceConfig->lengthSeconds); i++) {
        auto samplePerPhase = 44100;
        for (auto s = 0; s < samplePerPhase; s++) {
            auto val0 = genSaw(s, samplePerPhase);
            auto val1 = buffer.getSample(0, s + (samplePerPhase * i));
            auto margin = 0.000001f;
            if (val0 != Catch::Approx(val1).margin(margin)) {
            }
            REQUIRE(val0 == Catch::Approx(val1).margin(margin));
        }
    }
}


SCENARIO("bounce loop scenario", "[engine][bounce][transport][loop]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());
    
    auto testFile = createSlowSawAudioFile();
    jassert(testFile.existsAsFile());
    std::cout << "Testfile: " << testFile.getFullPathName() << std::endl;
    
    GIVEN("generated audio file with loop")
    {
        auto engine = AudiumFactory::createAudiumEngine();
    auto store = engine->getProjectFileStore();
        auto ok = store->open(testFile, nullptr);
        REQUIRE(ok);
        engine->getPlayListScheduler()->commitPlayListData();
        auto loop = engine->getPlayListScheduler()->getTransportLoop();
        loop->setLoopActive(true);
        loop->setLoopPositionRange(nullptr, {1.0, 2.0}, audium::seconds);
 
        auto bounceConfig = std::make_shared<audium::ExportAudioConfig>();
        bounceConfig->fileName = File(String(CURRENT_SOURCE_DIR) + String("/TestFiles/slow-saw-out.wav"));
        bounceConfig->sampleRate = 44100.0;
        bounceConfig->blockSize = 64;
        bounceConfig->lengthSeconds = 60.0;
        
        auto exporter = std::make_unique<AudioExporter>(*engine, bounceConfig);
        
        WHEN("bouncing session")
        {
            exporter->bounce();

            THEN("examine bounced audio file")
            {
                examineBouncedAudioFile(bounceConfig);
            }
        }
        WHEN("bouncing session with length to match loop end")
        {
            auto track = engine->getAudioTrackContainer()->getAudioTracks().front();
            auto item = track->getPlayListContainer()->getPlayListItem(0);
            item->setLength(2.0, audium::seconds);

            exporter->bounce();

            THEN("examine bounced audio file")
            {
                examineBouncedAudioFile(bounceConfig);
            }
        }
        WHEN("bouncing session with start and end to match loop")
        {
            auto track = engine->getAudioTrackContainer()->getAudioTracks().front();
            auto item = track->getPlayListContainer()->getPlayListItem(0);
            item->setAbsolutePosition(1.0, audium::seconds);
            item->getRegion()->setRegionData({1.0, 2.0}, audium::seconds);

            exporter->bounce();

            THEN("examine bounced audio file")
            {
                examineBouncedAudioFile(bounceConfig);
            }
        }
        
        
        if (bounceConfig->fileName.existsAsFile())
            bounceConfig->fileName.deleteFile();
        
        engine = nullptr;
    }
    
    
    if (testFile.existsAsFile())
        testFile.deleteFile();

    
    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}

