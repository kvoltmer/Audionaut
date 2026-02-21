#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Factory/AudioTrackFactory.h"
#include "Engine/Resource/ChannelMapping.h"
#include "Engine/AudioSources/AudiumTransportSource.h"
#include "Engine/Export/AudioExportThread.h"
#include "Engine/PlayList/PlayListScheduler.h"

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


SCENARIO("bounce loop scenario", "[engine][bounce][transport][loop]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());
    
    
    
    auto fileUnderTest = File(String(CURRENT_SOURCE_DIR) + String("/TestFiles/Sessions/simple-sine-loop.audium"));
    REQUIRE(fileUnderTest.exists());
    
    GIVEN("Load session file (audio file starts: at second 1 with 1 second duration)")
    {
        auto engine     = AudiumFactory::createAudiumEngine();
        auto ok = engine->openFile(fileUnderTest, nullptr);
        REQUIRE(ok);
        
        WHEN("bouncing session")
        {
            auto bounceConfig = std::make_shared<audium::ExportAudioConfig>();
            
            bounceConfig->fileName = juce::File::createTempFile(".wav");
            bounceConfig->sampleRate = 44100.0;
            //bounceConfig->positionSeconds = 1.5;
            bounceConfig->positionSeconds = 0.0;
            
            auto totalLength = engine->getPlayListScheduler()->getTotalLength(audium::seconds);
            totalLength -= bounceConfig->positionSeconds;
            
            // bounce to file
            auto exporter = std::make_unique<AudioExportThread>(*engine, bounceConfig);
            exporter->bounce();
            std::cout << "bounceToFile -> " << bounceConfig->fileName.getFullPathName() << std::endl;
            exporter = nullptr;

#if 0

            THEN("examine bounced audio file")
            {

                auto mag = 0.0;
                AudioFormatManager formatManager;
                formatManager.registerBasicFormats();
                std::unique_ptr<AudioFormatReader> reader;
                reader.reset(formatManager.createReaderFor(bounceConfig->fileName));

                if (reader != nullptr)
                {
                    AudioBuffer<float> buffer((int)reader->numChannels, (int)reader->lengthInSamples);
                    
                    auto success = reader->read(&buffer,
                                                0,
                                                (int)reader->lengthInSamples,
                                                0,
                                                true,
                                                true);
                    // read should be successful
                    REQUIRE(success);
                    mag = buffer.getMagnitude(0, buffer.getNumSamples());
                    // bounced audio file should not be empty
                    REQUIRE(mag > 0.0);
                    
                    
                    auto sessionStart = 1.0; // the 1st clip starts at second 1
                    
                    auto samplesUntilStart = static_cast<int>(bounceConfig->sampleRate * (sessionStart - bounceConfig->positionSeconds));
                    
                    if (samplesUntilStart > 0) {
                        // magnitue of 1st part is 0.0
                        mag = buffer.getMagnitude(0, samplesUntilStart - 1);
                        REQUIRE(mag == Catch::Approx(0.0));
                    }
                    
                    // magnitue of sample at 2nd part > 0.0
                    if (samplesUntilStart < 0)
                        samplesUntilStart = 0;
                    mag = buffer.getMagnitude(samplesUntilStart, 1);
                    
                    REQUIRE(mag > 0.0);
                    
                    auto sessionStart2 = 3.0; // the 2nd clip starts at second 3
                    auto samplesUntilStart2 = static_cast<int>(bounceConfig->sampleRate * (sessionStart2 - bounceConfig->positionSeconds));
                    auto durationGap = 1.0;
                    auto durationGapSamples = static_cast<int>(bounceConfig->sampleRate * durationGap);
                    
                    
                    mag = buffer.getMagnitude(samplesUntilStart2 - durationGapSamples, durationGapSamples - 1);
                    REQUIRE(mag == Catch::Approx(0.0));
                    
                    if (samplesUntilStart < 0)
                        samplesUntilStart = 0;
                    mag = buffer.getMagnitude(samplesUntilStart2, 1);
                    REQUIRE(mag > 0.0);
                    
                    
                    // total length
                    REQUIRE(reader->lengthInSamples == static_cast<unsigned int>(bounceConfig->sampleRate * totalLength));
                    
                }

                
            }
            
            if (bounceConfig->fileName.existsAsFile())
                bounceConfig->fileName.deleteFile();
            
#endif
        }
        
        engine = nullptr;
        
        
    }

    
    
    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}

