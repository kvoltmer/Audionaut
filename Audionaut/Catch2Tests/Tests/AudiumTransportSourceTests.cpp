#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Factory/AudioTrackFactory.h"
#include "Engine/Resource/ChannelMapping.h"
#include "Engine/AudioSources/AudiumTransportSource.h"
#include "Engine/Export/AudioExportThread.h"
#include "Engine/PlayList/PlayListScheduler.h"

#include "TestUtils.h"

using namespace audium;
using namespace juce;

SCENARIO("tranport source scenario", "[engine][dsp][transport]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());
    
    
    auto fileUnderTest = File(String(CURRENT_SOURCE_DIR) + String("/TestFiles/Sessions/120-funk-export-v2.audium"));
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
                        // 1st second is silence
                        mag = buffer.getMagnitude(0, samplesUntilStart - 1);
                        REQUIRE(mag == Catch::Approx(0.0));
                    }
                    
                    // mag > 0 at second 1
                    if (samplesUntilStart < 0)
                        samplesUntilStart = 0;
                    mag = buffer.getMagnitude(samplesUntilStart, 1);
                    REQUIRE(mag > 0.0);
                    
                    auto sr = bounceConfig->sampleRate;
                    
                    // silence at second 2
                    mag = buffer.getMagnitude(static_cast<int>(2.0 * sr) , 1);
                    REQUIRE(mag <= 0.0);

                    // mag > 0 at second 3
                    mag = buffer.getMagnitude(static_cast<int>(3.0 * sr) , 1);
                    REQUIRE(mag > 0.0);
                    
                    // total length
                    REQUIRE(reader->lengthInSamples == static_cast<unsigned int>(bounceConfig->sampleRate * totalLength));
                    
                }
                
            }
            
            if (bounceConfig->fileName.existsAsFile())
                bounceConfig->fileName.deleteFile();
        }
        
        engine = nullptr;
    }

    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}

static void examineFile(std::shared_ptr<audium::ExportAudioConfig> bounceConfig)
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
    
    // once sec ramp
    auto samplePerPhase = 44100;
    for (auto s = 0; s < samplePerPhase; s++) {
        auto val0 = genSaw(s, samplePerPhase);
        auto val1 = buffer.getSample(0, s);
        
        if (val0 != Catch::Approx(val1).margin(0.000001f)) {
        }
        REQUIRE(val0 == Catch::Approx(val1).margin(0.000001f));
    }
    // one sec silence
    for (auto s = 0; s < samplePerPhase; s++) {
        auto val1 = buffer.getSample(0, s + samplePerPhase);
        REQUIRE(0.f == Catch::Approx(val1).margin(0.000001f));
    }
    
    
}

SCENARIO("tranport source duration scenario", "[engine][dsp][transport][duration]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());
    
    auto testFile = createSlowSawTwoSecondsAudioFile();
    jassert(testFile.existsAsFile());
    std::cout << "Testfile: " << testFile.getFullPathName() << std::endl;
    
    GIVEN("engine loading the audio file")
    {
        auto engine = AudiumFactory::createAudiumEngine();
        auto ok = engine->openFile(testFile, nullptr);
        REQUIRE(ok);
        engine->getPlayListScheduler()->commitPlayListData();
        
        auto bounceConfig = std::make_shared<audium::ExportAudioConfig>();
        bounceConfig->fileName = File(String(CURRENT_SOURCE_DIR) + String("/TestFiles/out.wav"));
        bounceConfig->sampleRate = 44100.0;
        bounceConfig->blockSize = 64;
        bounceConfig->lengthSeconds = 3.0;
        
        auto exporter = std::make_unique<AudioExportThread>(*engine, bounceConfig);
        
        WHEN("bouncing session")
        {
            auto track = engine->getAudioTrackContainer()->getAudioTracks().front();
            jassert(track);
            auto item = track->getPlayListContainer()->getPlayListItem(0);
            jassert(item);
            item->setLength(1.0, audium::seconds);
            exporter->bounce();

            THEN("examine bounced audio file")
            {
                examineFile(bounceConfig);
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

