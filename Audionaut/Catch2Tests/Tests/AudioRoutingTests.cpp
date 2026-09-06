#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Project/ProjectSerializer.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/Playback/AudioBusInterface.h"

#include "TestUtils.h"

using namespace audium;
using namespace juce;

namespace
{
constexpr auto kBlockSize = 64;
constexpr auto kSampleRate = 48000.0;
constexpr auto kBusChannels = 4;

// DC value fed into hardware input channel i, so inputs are distinguishable
float inputValue(int i) { return 0.1f * (float)(i + 1); }

struct RoutingHarness
{
    RoutingHarness(int numInputs, int numOutputs) :
        engine(AudiumFactory::createAudiumEngine()),
        inBuffer(numInputs, kBlockSize),
        outBuffer(numOutputs, kBlockSize)
    {
        engine->getProjectSerializer()->createNewProject(1);
        audioBusInterface = engine->getPlayListScheduler()->getAudioBusInterface();
        audioBusInterface->setNumAudioBusChannels(kBusChannels);

        for (auto i = 0; i < numInputs; ++i) {
            for (auto s = 0; s < kBlockSize; ++s) {
                inBuffer.setSample(i, s, inputValue(i));
            }
        }
        outBuffer.clear();
    }

    // channel data reaches the renderer through the lock-free fifo, pumped
    // synchronously in the headless test binary
    void setChannel(int busChannel, AudioChannelData data)
    {
        data.channelNumber = busChannel; // normally stamped by AudioChannel::commitChannelData
        audioBusInterface->setChannelData(busChannel, data);
    }

    void process()
    {
        audioBusInterface->prepareToPlay(kBlockSize, kSampleRate);

        juce::dsp::AudioBlock<const float> inBlock(inBuffer);
        juce::dsp::AudioBlock<float> outBlock(outBuffer);
        juce::dsp::ProcessContextNonReplacing<float> context(inBlock, outBlock);
        audioBusInterface->process(context);
    }

    float outputLevel(int channel) const
    {
        return outBuffer.findMinMax(channel, 0, kBlockSize).getEnd();
    }

    std::shared_ptr<AudiumEngine> engine;
    std::shared_ptr<AudioBusInterface> audioBusInterface;
    AudioBuffer<float> inBuffer;
    AudioBuffer<float> outBuffer;
};
} // namespace

SCENARIO("input routing scenario", "[engine][routing]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    GIVEN("a 4-channel bus with 4 hardware inputs and a stereo output")
    {
        RoutingHarness harness(4, 2);

        WHEN("bus channel 0 monitors an explicitly routed input 2")
        {
            AudioChannelData d;
            d.monitor = true;
            d.inputChannel = 2;
            harness.setChannel(0, d);
            harness.process();

            THEN("bus channel 0 carries input 2, the other bus channels stay silent")
            {
                REQUIRE(harness.audioBusInterface->getChannelLevel(0)
                        == Catch::Approx(inputValue(2)));
                REQUIRE(harness.audioBusInterface->getChannelLevel(1) == Catch::Approx(0.f));
                REQUIRE(harness.audioBusInterface->getChannelLevel(2) == Catch::Approx(0.f));
                REQUIRE(harness.audioBusInterface->getChannelLevel(3) == Catch::Approx(0.f));
            }
        }

        WHEN("bus channel 1 monitors with the default input")
        {
            AudioChannelData d;
            d.monitor = true; // inputChannel stays -1
            harness.setChannel(1, d);
            harness.process();

            THEN("it is fed by hardware input 1, the legacy implicit mapping")
            {
                REQUIRE(harness.audioBusInterface->getChannelLevel(1)
                        == Catch::Approx(inputValue(1)));
            }
        }

        WHEN("a record-armed channel routes from input 3")
        {
            AudioChannelData d;
            d.record = true;
            d.inputChannel = 3;
            harness.setChannel(0, d);
            harness.process();

            THEN("the recording level meters input 3")
            {
                REQUIRE(harness.audioBusInterface->getRecordingLevel(0)
                        == Catch::Approx(inputValue(3)));
            }
        }

        WHEN("a channel routes from an input the device does not have")
        {
            AudioChannelData d;
            d.monitor = true;
            d.record = true;
            d.inputChannel = 10;
            harness.setChannel(0, d);
            harness.process();

            THEN("the channel stays silent and nothing crashes")
            {
                REQUIRE(harness.audioBusInterface->getChannelLevel(0) == Catch::Approx(0.f));
                REQUIRE(harness.audioBusInterface->getRecordingLevel(0) == Catch::Approx(0.f));
            }
        }
    }
}

SCENARIO("output routing scenario", "[engine][routing]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    GIVEN("a 4-channel bus with 4 hardware inputs and 4 hardware outputs")
    {
        RoutingHarness harness(4, 4);

        WHEN("a Main channel is panned hard left")
        {
            AudioChannelData d;
            d.monitor = true;
            d.pan = -1.f; // outputChannel stays -1 -> Main mix
            harness.setChannel(0, d);
            harness.process();

            THEN("output 0 is hot, output 1 is silent, and the master meter registers")
            {
                REQUIRE(harness.outputLevel(0) == Catch::Approx(inputValue(0)).margin(0.001));
                REQUIRE(harness.outputLevel(1) == Catch::Approx(0.f).margin(0.001));
                REQUIRE(harness.audioBusInterface->getMasterLevel(0) > 0.f);
            }
        }

        WHEN("a channel routes directly to output 3")
        {
            AudioChannelData d;
            d.monitor = true;
            d.outputChannel = 3;
            harness.setChannel(0, d);
            harness.process();

            THEN("output 3 carries the unpanned signal, the Main outputs and meter stay silent")
            {
                REQUIRE(harness.outputLevel(3) == Catch::Approx(inputValue(0)));
                REQUIRE(harness.outputLevel(0) == Catch::Approx(0.f));
                REQUIRE(harness.outputLevel(1) == Catch::Approx(0.f));
                REQUIRE(harness.audioBusInterface->getMasterLevel(0) == Catch::Approx(0.f));
                REQUIRE(harness.audioBusInterface->getMasterLevel(1) == Catch::Approx(0.f));
            }
        }

        WHEN("a channel routes to an output the device does not have")
        {
            AudioChannelData d;
            d.monitor = true;
            d.outputChannel = 10;
            harness.setChannel(0, d);
            harness.process();

            THEN("the signal is dropped from every output and nothing crashes")
            {
                for (auto c = 0; c < 4; ++c) {
                    REQUIRE(harness.outputLevel(c) == Catch::Approx(0.f));
                }
            }
        }

        WHEN("stem export is enabled")
        {
            for (auto k = 0; k < kBusChannels; ++k) {
                AudioChannelData d;
                d.monitor = true;
                d.outputChannel = 0; // routing must be ignored during stem export
                harness.setChannel(k, d);
            }
            harness.audioBusInterface->setStemExport(true);
            harness.process();

            THEN("each output carries its bus channel identity-mapped")
            {
                for (auto c = 0; c < 4; ++c) {
                    REQUIRE(harness.outputLevel(c) == Catch::Approx(inputValue(c)));
                }
            }
        }
    }
}
