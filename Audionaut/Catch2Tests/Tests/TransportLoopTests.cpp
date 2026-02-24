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
            transportLoop->setLoopPositionRange(nullptr, {0.0, 1.0}, audium::seconds);
            auto transportPos = 0.0;
            auto delta = 0.1; // seconds
            
            for (auto i = 0; i < 1000; i++) {
                
                auto samples = static_cast<int>(44100.0 * delta);
                auto result = transportLoop->processLoop(transportPos, samples);
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

static float genSaw(int s, int w)
{
    return (static_cast<float>(s) / static_cast<float>(w - 1) * 2.f) - 1.f;
}

static const File createTestFile()
{
    auto targetFile = File(String(CURRENT_SOURCE_DIR) + String("/TestFiles/slow-saw.wav"));
    TemporaryFile tempFile (targetFile);
    
    std::unique_ptr<OutputStream> stream (tempFile.getFile().createOutputStream());
    jassert(stream);
    if (stream != nullptr) {
        WavAudioFormat wav;
        auto opt = AudioFormatWriter::Options{}.withSampleRate (44100.0)
            .withNumChannels (1)
            .withBitsPerSample (32)
            .withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);
        auto writer = wav.createWriterFor (stream, opt);
        jassert(writer);
        if (writer != nullptr) {
            
            auto blockSize = 44100;
            
            AudioBuffer<float> buffer(1, blockSize);
            AudioSourceChannelInfo info (&buffer, 0, blockSize);
            
            // 1st second silence
            info.clearActiveBufferRegion();
            writer->writeFromAudioSampleBuffer(*info.buffer, info.startSample, info.numSamples);
            
            // 2nd second generate saw [-1, +1]
            for (auto s = 0; s < blockSize; s++) {
                *info.buffer->getWritePointer(0, s) = genSaw(s, blockSize);
            }
            writer->writeFromAudioSampleBuffer(*info.buffer, info.startSample, info.numSamples);
            
            // 3rd second silence
            info.clearActiveBufferRegion();
            writer->writeFromAudioSampleBuffer(*info.buffer, info.startSample, info.numSamples);
            
            writer.reset();
            tempFile.overwriteTargetFileWithTemporary();
            return tempFile.getTargetFile();
        }
    }
    return File();
}


SCENARIO("bounce loop scenario", "[engine][bounce][transport][loop]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());
    
    auto audioFile = createTestFile();
    jassert(audioFile.existsAsFile());
    std::cout << "Testfile: " << audioFile.getFullPathName() << std::endl;
    
    auto outFile = File(String(CURRENT_SOURCE_DIR) + String("/TestFiles/out-slow-saw.wav"));
    
    GIVEN("Load session file (audio file starts: at second 1 with 1 second duration)")
    {
        auto engine = AudiumFactory::createAudiumEngine();
        auto ok = engine->openFile(audioFile, nullptr);
        REQUIRE(ok);
        engine->getPlayListScheduler()->commitPlayListData();
        auto loop = engine->getPlayListScheduler()->getTransportLoop();
        loop->setLoopActive(true);
        loop->setLoopPositionRange(nullptr, {1.0, 2.0}, audium::seconds);
        
        
        WHEN("bouncing session")
        {
            auto bounceConfig = std::make_shared<audium::ExportAudioConfig>();
            
            bounceConfig->fileName = File(String(CURRENT_SOURCE_DIR) + String("/TestFiles/slow-saw-out.wav"));
            bounceConfig->sampleRate = 44100.0;

            auto totalLength = engine->getPlayListScheduler()->getTotalLength(audium::seconds);
            REQUIRE(totalLength == Catch::Approx(3.0));
            
            bounceConfig->lengthSeconds = 6.0;
            
            // bounce to file
            auto exporter = std::make_unique<AudioExportThread>(*engine, bounceConfig);
            exporter->bounce();
            std::cout << "bounceToFile -> " << bounceConfig->fileName.getFullPathName() << std::endl;
            exporter = nullptr;

            THEN("examine bounced audio file")
            {
                AudioFormatManager formatManager;
                formatManager.registerBasicFormats();
                std::unique_ptr<AudioFormatReader> reader;
                reader.reset(formatManager.createReaderFor(bounceConfig->fileName));

                if (reader != nullptr) {
                    AudioBuffer<float> buffer((int)reader->numChannels, (int)reader->lengthInSamples);
                    
                    auto success = reader->read(&buffer,
                                                0,
                                                (int)reader->lengthInSamples,
                                                0,
                                                true,
                                                true);
                    // read should be successful
                    REQUIRE(success);
                    
                    auto samplePerPhase = 44100;
                    auto iterations = 6;
                    for (auto i = 1; i < iterations; i++) {
                        
                        for (auto s = 0; s < samplePerPhase; s++) {
                            auto val0 = genSaw(s, samplePerPhase);
                            auto val1 = buffer.getSample(0, s + (samplePerPhase * i));
                            auto m = 0.000001f;
                            REQUIRE(val0 == Catch::Approx(val1).margin(m));
                            
                        }
                    }
                }
            }
#if 0
            if (bounceConfig->fileName.existsAsFile())
                bounceConfig->fileName.deleteFile();
#endif
        }
        engine = nullptr;
    }

    
    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}

