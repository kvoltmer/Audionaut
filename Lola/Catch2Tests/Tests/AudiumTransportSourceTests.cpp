#include <catch2/catch_test_macros.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Factory/AudioTrackFactory.h"
#include "Engine/Resource/ChannelMapping.h"
#include "Engine/AudioSources/AudiumTransportSource.h"
#include "Engine/Export/AudioExportThread.h"
#include "Engine/PlayList/PlayListScheduler.h"

using namespace audium;

SCENARIO("tranport source scenario", "[engine][dsp][transport]")
{
    juce::MessageManager::getInstance();
    juce::MessageManagerLock mmLock(Thread::getCurrentThread());

    GIVEN("Load session file (audio file starts: at second 1 with 1 second duration)")
    {
        auto engine     = AudiumFactory::createAudiumEngine();
        engine->openFile(juce::File("../../../TestFiles/Sessions/120-funk-export.audium"), nullptr);

        WHEN("bouncing session")
        {
                        
            audium::ExportAudioConfig bounceConfig;
            bounceConfig.fileName = juce::File::createTempFile(".wav");
            bounceConfig.sampleRate = 44100.0;
            //bounceConfig.positionSeconds = 1.5;
            bounceConfig.positionSeconds = 0.0;
            
            auto totalLength = engine->getPlayListScheduler()->getTotalLength(audium::seconds);
            totalLength -= bounceConfig.positionSeconds;
            
            // bounce to file
            auto exporter = std::make_unique<AudioExportThread>(*engine, bounceConfig);
            exporter->bounceToFile(bounceConfig);
            std::cout << "bounceToFile -> " << bounceConfig.fileName.getFullPathName() << std::endl;
            exporter = nullptr;
            

            THEN("examine bounced audio file")
            {
                AudioFormatManager formatManager;
                formatManager.registerBasicFormats();
                std::unique_ptr<AudioFormatReader> reader;
                reader.reset(formatManager.createReaderFor(bounceConfig.fileName));

                if (reader != nullptr)
                {
                    AudioBuffer<float> buffer((int)reader->numChannels, (int)reader->lengthInSamples);
                    
                    reader->read(   &buffer,
                                    0,
                                    (int)reader->lengthInSamples,
                                    0,
                                    true,
                                    true);
                    
                    auto sessionStart = 1.0; // the 1st clip starts at second 1
                    
                    auto samplesUntilStart = static_cast<int>(bounceConfig.sampleRate * (sessionStart - bounceConfig.positionSeconds));
                    auto mag = 0.0;
                    if (samplesUntilStart > 0) {
                        // magnitue of 1st part is 0.0
                        mag = buffer.getMagnitude(0, samplesUntilStart - 1);
                        REQUIRE(mag <= 0.0);
                    }
                    
                    // magnitue of sample at 2nd part > 0.0
                    if (samplesUntilStart < 0)
                        samplesUntilStart = 0;
                    mag = buffer.getMagnitude(samplesUntilStart, 1);
                    REQUIRE(mag > 0.0);
                    

                    
                    
                    auto sessionStart2 = 3.0; // the 2nd clip starts at second 3
                    auto samplesUntilStart2 = static_cast<int>(bounceConfig.sampleRate * (sessionStart2 - bounceConfig.positionSeconds));
                    auto durationGap = 1.0;
                    auto durationGapSamples = static_cast<int>(bounceConfig.sampleRate * durationGap);
                    
                    
                    mag = buffer.getMagnitude(samplesUntilStart2 - durationGapSamples, durationGapSamples - 1);
                    REQUIRE(mag <= 0.0);
                    
                    if (samplesUntilStart < 0)
                        samplesUntilStart = 0;
                    mag = buffer.getMagnitude(samplesUntilStart2, 1);
                    REQUIRE(mag > 0.0);
                    
                    
                    // total length
                    REQUIRE(reader->lengthInSamples == static_cast<unsigned int>(bounceConfig.sampleRate * totalLength));
                    
                }
                
            }
        }
        
        engine = nullptr;
    }

    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}


