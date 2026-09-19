#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Project/ProjectSerializer.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Factory/AudioTrackFactory.h"
#include "Engine/Resource/ChannelMapping.h"
#include "Engine/AudioSources/VoiceSource.h"
#include "Engine/Export/AudioExporter.h"
#include "Engine/PlayList/PlayListScheduler.h"

#include "Engine/PlayList/TransportLoop.h"
#include "Engine/Provider/TempoProvider.h"
#include "Engine/Recording/RecordingActionHandler.h"
#include "Engine/Channel/AudioChannel.h"

#include "TestUtils.h"

using namespace audium;
using namespace juce;


SCENARIO("recording scenario", "[engine][recording]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());
    
    auto sr = 44100.0;
    // create a slow saw (5 seconds)
    auto inputFile = createSlowSawAudioFile(5, false, false);
    auto inBuffer = audioFileToAudioBuffer(inputFile);
    // remove second at end
    
    auto bounceConfig = std::make_shared<audium::ExportAudioConfig>();
    bounceConfig->fileName = File(String(CURRENT_SOURCE_DIR) + String("/TestFiles/rec-out.wav"));
    bounceConfig->sampleRate = sr;
    bounceConfig->blockSize = 1024 * 4;
    bounceConfig->lengthSeconds = (double)inBuffer.getNumSamples() / bounceConfig->sampleRate;
    bounceConfig->numChannels = 1;



    GIVEN("generated audio file with loop")
    {
        auto engine = AudiumFactory::createAudiumEngine();
        
        engine->getProjectSerializer()->createNewProject(1);
        auto loop = engine->getPlayListScheduler()->getTransportLoop();
        loop->setLoopActive(true);
        
        // TODO: test loop start pos 0
        loop->setLoopPositionRange(nullptr, {1.0, 2.0}, audium::seconds);
        
        WHEN("recording")
        {
            
            auto recordingLength = 4.25;
            engine->getPlayListScheduler()->recordFromAudioBuffer(inBuffer,
                                                                  recordingLength);
            
            
            // chech if the first 4 seconds are recorded properly
            auto recordedFile = engine->getPlayListScheduler()->getAudioBusInterface()->getRecordedAudioFile(0);
            auto recBuffer = audioFileToAudioBuffer(recordedFile);
            REQUIRE(recBuffer.getNumSamples() == int(recordingLength * sr));

            // The waveform thumbnail is fed through a fifo drained off the
            // audio thread; stopping the take flushes it, so every recorded
            // sample must have reached the thumbnail (rounded up to its
            // 64-sample resolution) and none may have been skipped.
            auto thumbnail = engine->getPlayListScheduler()->getAudioBusInterface()->getRecordingThumbnail(0);
            REQUIRE(thumbnail != nullptr);
            const auto recordedSamples = static_cast<int64>(recordingLength * sr);
            REQUIRE(thumbnail->getNumSamplesFinished() >= recordedSamples);
            REQUIRE(thumbnail->getNumSamplesFinished() < recordedSamples + 64);
            for (auto i = 0; i < (int)recordingLength; i++) {
                auto samplePerPhase = 44100;
                for (auto s = 0; s < samplePerPhase; s++) {
                    auto val0 = genSaw(s, samplePerPhase);
                    auto val1 = recBuffer.getSample(0, s + (samplePerPhase * i));
                    auto margin = 0.000001f;
                    REQUIRE(val0 == Catch::Approx(val1).margin(margin));
                }
            }
            
            engine->getRecordingActionHandler()->onRecordingFinished();
            engine->getPlayListScheduler()->commitPlayListData();

            auto exporter = std::make_unique<AudioExporter>(*engine, bounceConfig);
            exporter->bounce();
            
            THEN("examine bounced audio file")
            {

                
                auto buffer = audioFileToAudioBuffer(bounceConfig->fileName);

                
                for (auto i = 0; i < static_cast<int>(bounceConfig->lengthSeconds); i++) {
                    auto samplePerPhase = 44100;
                    for (auto s = 0; s < samplePerPhase; s++) {
                        auto val0 = genSaw(s, samplePerPhase);
                        auto val1 = buffer.getSample(0, s + (samplePerPhase * i));
                        auto margin = 0.000001f;
                        REQUIRE(val0 == Catch::Approx(val1).margin(margin));
                    }
                }
            }
        }
        
        engine = nullptr;
    }

    if (bounceConfig->fileName.existsAsFile())
        bounceConfig->fileName.deleteFile();
    
    if (inputFile.existsAsFile())
        inputFile.deleteFile();

    
    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}


// isRecordEnabled/isRecording took channel 0 for "any channel" while
// setRecordEnabled took it for the first channel, so asking about channel
// 0 answered for the whole track.
SCENARIO("record-enable is queried per channel, channel 0 included", "[engine][recording]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    GIVEN("a stereo track with only its second channel armed")
    {
        auto engine = AudiumFactory::createAudiumEngine();
        engine->getProjectSerializer()->createNewProject(2);
        auto track = engine->getAudioTrackContainer()->getAudioTrack(0);
        REQUIRE(track != nullptr);
        REQUIRE(track->getNumAudioTrackChannels() == 2);

        // the headless engine has a single input; route channel 1 to it
        track->getChannel(1)->setInputChannel(0);
        track->setRecordEnabled(1, true);
        // record-enable goes through the lock-free commander, which the
        // audio thread would drain; here the test drains it
        engine->getPlayListScheduler()->getAudioBusInterface()->invokeCommands();

        THEN("channel 0 reports not enabled while channel 1 and the track do")
        {
            REQUIRE_FALSE(track->isRecordEnabled(0));
            REQUIRE(track->isRecordEnabled(1));
            REQUIRE(track->isRecordEnabled());
            REQUIRE_FALSE(track->isRecording(0));
            REQUIRE_FALSE(track->isRecording());
        }

        // the track must not outlive its engine
        track = nullptr;
        engine = nullptr;
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}
