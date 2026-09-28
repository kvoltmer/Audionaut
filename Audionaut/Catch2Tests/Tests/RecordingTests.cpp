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
#include "Engine/PlayList/ClipTempo.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/Region/AudioRegion.h"

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

// Arming used to hand a freshly made AudioRecorder to the audio thread, which
// inserted it into a std::map inside the callback, and disarming erased it
// there - running the writer flush and the disk thread's join in the audio
// callback while the message thread read the same map. Recorders are now
// built and retired on the message thread; the audio thread is only handed
// a pointer and acknowledges dropping it before the object is freed.
SCENARIO("recorders are never built or torn down inside a processed block", "[engine][recording]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    GIVEN("a mono track fed with blocks by hand")
    {
        auto engine = AudiumFactory::createAudiumEngine();
        engine->getProjectSerializer()->createNewProject(1);
        auto scheduler = engine->getPlayListScheduler();
        auto bus = scheduler->getAudioBusInterface();
        auto track = engine->getAudioTrackContainer()->getAudioTrack(0);
        REQUIRE(track != nullptr);
        REQUIRE(AudioRecorder::getNumInstances() == 0);

        // like a running device: channel commands are drained by process()
        // only, not pumped inline by the headless engine
        bus->setPumpsCommandsSynchronously(false);

        const auto blockSize = 512;
        const auto sampleRate = 44100.0;
        scheduler->prepareToPlay(blockSize, sampleRate);
        scheduler->setAbsoluteStartPosition(0.0, audium::seconds);

        AudioBuffer<float> inBuffer(1, blockSize);
        AudioBuffer<float> outBuffer(1, blockSize);
        juce::dsp::AudioBlock<float> inBlock (inBuffer);
        juce::dsp::AudioBlock<float> outBlock (outBuffer);
        juce::dsp::ProcessContextNonReplacing<float> context (inBlock, outBlock);
        for (auto s = 0; s < blockSize; ++s)
            inBuffer.setSample(0, s, 0.5f);

        auto positionBeats = 0.0;
        const auto beatsPerBlock = TempoProvider::clocksToBeats(
            scheduler->getTempoProvider()->secondsToClocks(static_cast<double>(blockSize) / sampleRate));

        // one block through the engine the way the device callback does it,
        // asserting that no recorder appears or disappears while it runs
        auto processBlock = [&] {
            const auto before = AudioRecorder::getNumInstances();
            outBlock.clear();
            scheduler->process(context, true, positionBeats, blockSize);
            REQUIRE(AudioRecorder::getNumInstances() == before);
            positionBeats += beatsPerBlock;
            juce::Thread::sleep (1); // let the disk thread keep up
        };

        WHEN("the channel is armed, recorded and disarmed three times over")
        {
            std::vector<juce::File> takes;
            std::vector<int> takeLengths;

            for (auto cycle = 0; cycle < 3; ++cycle) {
                // arming builds the recorder here, synchronously; the one
                // retired last time has been freed by now - it went as soon
                // as the audio thread had acknowledged dropping it
                track->setRecordEnabled(0, true);
                REQUIRE(AudioRecorder::getNumInstances() == 1);
                REQUIRE(track->isRecordEnabled(0));
                processBlock(); // drains the arm command

                const auto blocks = 20 + 10 * cycle;
                scheduler->setRecordingArmed(true);
                scheduler->startRecording();
                scheduler->startPlaying();
                REQUIRE(track->isRecording(0));
                for (auto b = 0; b < blocks; ++b)
                    processBlock();

                scheduler->stopPlaying(); // stops the take: flush happens here, on this thread
                REQUIRE_FALSE(track->isRecording(0));
                takes.push_back(bus->getRecordedAudioFile(0));
                takeLengths.push_back(blocks * blockSize);

                // disarming retires the recorder but keeps it alive until the
                // audio thread has run the command that drops it
                track->setRecordEnabled(0, false);
                REQUIRE_FALSE(track->isRecordEnabled(0));
                REQUIRE(AudioRecorder::getNumInstances() == 1);
                processBlock(); // drains the disarm command - still no teardown in here
                REQUIRE(AudioRecorder::getNumInstances() == 1);
            }

            THEN("every take holds exactly the blocks fed while it ran")
            {
                REQUIRE(takes.size() == 3);
                for (size_t i = 0; i < takes.size(); ++i) {
                    REQUIRE(takes[i].existsAsFile());
                    auto recorded = audioFileToAudioBuffer(takes[i]);
                    REQUIRE(recorded.getNumSamples() == takeLengths[i]);
                    REQUIRE(recorded.getSample(0, 0) == Catch::Approx(0.5f).margin(0.0001f));
                    REQUIRE(recorded.getSample(0, takeLengths[i] - 1) == Catch::Approx(0.5f).margin(0.0001f));
                }
            }

            THEN("the last retired recorder goes with the engine, never in a block")
            {
                processBlock();
                REQUIRE(AudioRecorder::getNumInstances() == 1);
                track = nullptr;
                bus = nullptr;
                scheduler = nullptr;
                engine = nullptr;
                REQUIRE(AudioRecorder::getNumInstances() == 0);
            }
        }

        // nothing may outlive the engine
        track = nullptr;
        bus = nullptr;
        scheduler = nullptr;
        engine = nullptr;
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

// A clip that is still recording has a resource without a URL. Its path
// getter reports a placeholder that is not an absolute path, and passing
// that to juce::File asserts - which the UI's analysis refresh did on every
// layout pass during a take. The file accessor hands out an empty File for
// that state instead, and the tempo lookup skips such resources.
SCENARIO("a recording resource has no local file yet", "[engine][recording][analysis]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    GIVEN("a clip whose resource is still being recorded")
    {
        auto engine = AudiumFactory::createAudiumEngine();
        engine->getProjectSerializer()->createNewProject(1);
        auto track = engine->getAudioTrackContainer()->getAudioTrack(0);
        REQUIRE(track != nullptr);

        // mirrors RecordingActionHandler::onRecordingStarted: no URL, no reader
        auto resourceGroup = track->createNewResourceGroup();
        auto resource = track->getAudioResourceContainer().addAudioResource({}, nullptr, track, resourceGroup, 0, 0);
        auto item = track->createDefaultPlayListItem(resource, resourceGroup, 0.0, audium::seconds);
        REQUIRE(item != nullptr);
        REQUIRE(resource->isRecording());

        THEN("the resource reports an empty file rather than a placeholder path")
        {
            REQUIRE(resource->getLocalFile() == juce::File());
            REQUIRE_FALSE(juce::File::isAbsolutePath(resource->getFullPathName()));
        }

        THEN("the clip tempo lookup finds no source file")
        {
            REQUIRE(ClipTempo::sourceFile(*item) == juce::File());
        }

        item = nullptr;
        resource = nullptr;
        resourceGroup = nullptr;
        track = nullptr;
        engine = nullptr;
    }

    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}
