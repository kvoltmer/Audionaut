#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <thread>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Project/ProjectSerializer.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Channel/AudioChannel.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/Playback/AudioBusInterface.h"
#include "Engine/Playback/PlaybackDefines.h"

using namespace audium;

SCENARIO("the audio callback takes the bus channel count from a published value, not the track vectors",
         "[engine][audiobus][snapshot][stress]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());
    auto engine = AudiumFactory::createAudiumEngine();
    auto tracks = engine->getAudioTrackContainer();
    auto scheduler = engine->getPlayListScheduler();
    auto audioBus = engine->getAudioBusInterface();

    // a render thread drains the command fifo below, like a device would -
    // the headless default of pumping it inline would make two consumers
    audioBus->setPumpsCommandsSynchronously(false);

    GIVEN("a project whose channel count follows the tracks") {
        engine->getProjectSerializer()->createNewProject(2);
        REQUIRE(tracks->getNumAudioTrackChannels() == 2);
        REQUIRE(tracks->getPublishedNumAudioTrackChannels() == 2);

        constexpr int blockSize = 128;
        constexpr double sampleRate = 44100.0;
        scheduler->prepareToPlay(blockSize, sampleRate);

        AudioBuffer<float> in(2, blockSize), out(2, blockSize);
        in.clear();
        juce::dsp::AudioBlock<float> inBlock(in), outBlock(out);
        auto renderBlock = [&] {
            outBlock.clear();
            juce::dsp::ProcessContextNonReplacing<float> context(inBlock, outBlock);
            scheduler->process(context, false, 0.0, blockSize);
        };

        WHEN("channels and tracks come and go on the message thread") {
            auto track = tracks->createNewAudioTrack("added");
            track->addChannel();
            track->addChannel();
            track->addChannel();
            renderBlock();
            THEN("the next block renders every channel") {
                REQUIRE(tracks->getPublishedNumAudioTrackChannels() == 5);
                REQUIRE(audioBus->getNumAudioBusChannels() == 5);
            }

            track->deleteChannel(track->getChannel(1).get());
            renderBlock();
            THEN("a removed channel leaves the bus") {
                REQUIRE(tracks->getPublishedNumAudioTrackChannels() == 4);
                REQUIRE(audioBus->getNumAudioBusChannels() == 4);
            }

            tracks->deleteAudioTrack(track);
            renderBlock();
            THEN("a removed track takes its channels with it") {
                REQUIRE(tracks->getPublishedNumAudioTrackChannels() == 2);
                REQUIRE(audioBus->getNumAudioBusChannels() == 2);
            }

            tracks->cleanup();
            renderBlock();
            THEN("an emptied container renders no channel") {
                REQUIRE(tracks->getPublishedNumAudioTrackChannels() == 0);
                REQUIRE(audioBus->getNumAudioBusChannels() == 0);
            }
        }

        WHEN("tracks and channels are added and removed while another thread renders blocks") {
            std::atomic<bool> stop { false };
            std::atomic<int> blocksRendered { 0 };
            std::atomic<bool> busInRange { true };
            std::atomic<bool> outputSilent { true };

            std::thread renderThread([&] {
                while (! stop.load()) {
                    renderBlock();

                    // the bus is sized from the published count: never
                    // a torn vector size, never beyond the fixed arrays
                    const auto busChannels = audioBus->getNumAudioBusChannels();
                    if (busChannels < 0 || busChannels > MAX_AUDIO_CHANNELS)
                        busInRange = false;

                    // no clip, no monitoring: an empty mix, whatever the count
                    for (auto c = 0; c < out.getNumChannels(); ++c)
                        if (out.getMagnitude(c, 0, blockSize) != 0.f)
                            outputSilent = false;

                    ++blocksRendered;
                }
            });

            // Bounded churn well inside MAX_AUDIO_CHANNELS: the vectors
            // reallocate and erase under the render thread throughout.
            constexpr int iterations = 3000;
            constexpr int maxTracks = 6;
            constexpr int maxChannelsPerTrack = 4;
            for (auto i = 0; i < iterations; ++i) {
                switch (i % 5) {
                    case 0:
                    case 1:
                        if (tracks->getNumItems() < maxTracks) {
                            auto t = tracks->createNewAudioTrack(juce::String());
                            t->ensureNumChannels(1 + i % maxChannelsPerTrack);
                        }
                        break;
                    case 2:
                        if (auto t = tracks->getAudioTrack(i % std::max(1, tracks->getNumItems())))
                            if (t->getNumAudioTrackChannels() < maxChannelsPerTrack)
                                t->addChannel();
                        break;
                    case 3:
                        if (auto t = tracks->getAudioTrack(i % std::max(1, tracks->getNumItems())))
                            if (t->getNumAudioTrackChannels() > 0)
                                t->deleteChannel(t->getChannel(t->getNumAudioTrackChannels() - 1).get());
                        break;
                    case 4:
                        if (tracks->getNumItems() > 1)
                            tracks->deleteAudioTrack(tracks->getAudioTrack(i % tracks->getNumItems()));
                        break;
                }
                REQUIRE(tracks->getPublishedNumAudioTrackChannels() == tracks->getNumAudioTrackChannels());
            }

            // let the render thread see the final state for a few thousand
            // blocks, bounded in time
            const auto blocksAtEnd = blocksRendered.load();
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (blocksRendered.load() < blocksAtEnd + 2000 && std::chrono::steady_clock::now() < deadline)
                std::this_thread::yield();

            stop = true;
            renderThread.join();

            THEN("every block rendered a bus inside the fixed arrays and the last one matches the model") {
                REQUIRE(blocksRendered.load() > 2000);
                REQUIRE(busInRange.load());
                REQUIRE(outputSilent.load());
                REQUIRE(tracks->getNumAudioTrackChannels() == tracks->getPublishedNumAudioTrackChannels());
                REQUIRE(audioBus->getNumAudioBusChannels() == tracks->getNumAudioTrackChannels());
            }
        }
    }

    engine = nullptr;
    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}
