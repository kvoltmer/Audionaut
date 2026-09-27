#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Project/ProjectFileStore.h"
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

namespace
{
    // Counts the loop notifications the tempo provider broadcasts, in order.
    struct LoopMessageCounter : public juce::ActionListener
    {
        void actionListenerCallback (const juce::String& message) override
        {
            if (message == audium::transportLoopEntered)
                ++entered;
            else if (message == audium::transportLoopAction)
                ++wrapped;

            order.add (message);
        }

        int entered = 0;
        int wrapped = 0;
        juce::StringArray order;
    };
}

// processLoop() runs on the audio thread; the action messages it used to
// broadcast directly (a lock plus an allocation per listener) now leave
// through an AsyncUpdater. This guards the contract the listeners rely on:
// one message per event, entry before wraps, even when several wraps pile
// up before the message thread gets to run.
SCENARIO("loop wrap notifications leave the audio thread", "[engine][transport][loop][realtime]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    GIVEN("a TransportLoop with a listener on its tempo provider")
    {
        auto tempoProvider = std::make_shared<TempoProvider>(nullptr);
        auto transportLoop = std::make_unique<audium::TransportLoop>(nullptr,
                                                                     tempoProvider);
        LoopMessageCounter counter;
        tempoProvider->addActionListener(&counter);

        transportLoop->setLoopActive(true);
        transportLoop->prepareToPlay(512, 44100.0);
        // loop between 0 and 1
        transportLoop->setLoopPositionRange(nullptr, {0.0, 1.0}, audium::seconds);

        WHEN("the loop wraps three times before the message thread runs")
        {
            auto transportPos = 0.0;
            auto delta = 0.1; // seconds
            auto samples = static_cast<int>(44100.0 * delta);
            auto wraps = 0;

            for (auto i = 0; i < 31; i++) {
                auto result = transportLoop->processLoop(transportPos, samples, audium::clocks);
                if (result.loopEvent)
                    wraps++;

                transportPos += tempoProvider->secondsToClocks(delta);
            }
            REQUIRE(wraps == 3);

            THEN("nothing has been delivered synchronously")
            {
                REQUIRE(counter.entered == 0);
                REQUIRE(counter.wrapped == 0);
            }

            THEN("the message thread delivers every event once, the entry first")
            {
                MessageManager::getInstance()->runDispatchLoopUntil(50);

                REQUIRE(counter.entered == 1);
                REQUIRE(counter.wrapped == 3);
                REQUIRE(counter.order.size() == 4);
                REQUIRE(counter.order[0] == String(audium::transportLoopEntered));
            }
        }

        WHEN("the loop is left and re-entered across message thread runs")
        {
            // enter, wrap, ...
            auto transportPos = 0.0;
            auto samples = 4410;
            for (auto i = 0; i < 11; i++) {
                transportLoop->processLoop(transportPos, samples, audium::clocks);
                transportPos += tempoProvider->secondsToClocks(0.1);
            }
            MessageManager::getInstance()->runDispatchLoopUntil(50);
            REQUIRE(counter.entered == 1);
            REQUIRE(counter.wrapped == 1);

            // ... jump outside the loop, then back in: one more entry, no wrap
            transportLoop->reset();
            transportLoop->processLoop(tempoProvider->secondsToClocks(5.0), samples, audium::clocks);
            transportLoop->processLoop(tempoProvider->secondsToClocks(0.5), samples, audium::clocks);
            MessageManager::getInstance()->runDispatchLoopUntil(50);

            THEN("each entry is reported once")
            {
                REQUIRE(counter.entered == 2);
                REQUIRE(counter.wrapped == 1);
            }
        }

        tempoProvider->removeActionListener(&counter);
        transportLoop = nullptr;
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

