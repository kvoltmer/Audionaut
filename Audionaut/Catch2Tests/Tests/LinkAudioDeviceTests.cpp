#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Project/ProjectSerializer.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/Playback/AudioBusInterface.h"
#include "Engine/Link/LinkAudioDevice.h"

#include "TestUtils.h"

using namespace audium;
using namespace juce;

namespace
{
constexpr auto kBlockSize = 64;
constexpr auto kSampleRate = 48000.0;
constexpr auto kInputValue = 0.5f;

// Just enough of an AudioIODevice for audioDeviceAboutToStart: the callback
// itself is driven by hand, the device never runs.
struct FakeDevice : public AudioIODevice
{
    explicit FakeDevice(int numInputs_) :
        AudioIODevice("fake", "fake"), numInputs(numInputs_) {}

    StringArray getOutputChannelNames() override { return { "L", "R" }; }
    StringArray getInputChannelNames() override
    {
        StringArray names;
        for (auto i = 0; i < numInputs; ++i)
            names.add("in " + String(i + 1));
        return names;
    }
    Array<double> getAvailableSampleRates() override { return { kSampleRate }; }
    Array<int> getAvailableBufferSizes() override { return { kBlockSize }; }
    int getDefaultBufferSize() override { return kBlockSize; }
    String open(const BigInteger&, const BigInteger&, double, int) override { return {}; }
    void close() override {}
    bool isOpen() override { return true; }
    void start(AudioIODeviceCallback*) override {}
    void stop() override {}
    bool isPlaying() override { return false; }
    String getLastError() override { return {}; }
    int getCurrentBufferSizeSamples() override { return kBlockSize; }
    double getCurrentSampleRate() override { return kSampleRate; }
    int getCurrentBitDepth() override { return 32; }
    BigInteger getActiveOutputChannels() const override
    {
        BigInteger b; b.setRange(0, 2, true); return b;
    }
    BigInteger getActiveInputChannels() const override
    {
        BigInteger b; b.setRange(0, numInputs, true); return b;
    }
    int getOutputLatencyInSamples() override { return 0; }
    int getInputLatencyInSamples() override { return 0; }

    int numInputs;
};

struct DeviceHarness
{
    DeviceHarness() :
        engine(AudiumFactory::createAudiumEngine()),
        inBuffer(2, kBlockSize),
        outBuffer(2, kBlockSize)
    {
        engine->getProjectSerializer()->createNewProject(1);
        audioBusInterface = engine->getPlayListScheduler()->getAudioBusInterface();
        device = engine->getLinkAudioDevice();

        // bus channel 0 monitors hardware input 0 (channel data reaches the
        // renderer through the lock-free fifo, pumped synchronously here)
        AudioChannelData d;
        d.channelNumber = 0;
        d.monitor = true;
        d.inputChannel = 0;
        audioBusInterface->setChannelData(0, d);

        for (auto c = 0; c < inBuffer.getNumChannels(); ++c)
            for (auto s = 0; s < kBlockSize; ++s)
                inBuffer.setSample(c, s, kInputValue);
    }

    ~DeviceHarness()
    {
        device = nullptr;
        audioBusInterface = nullptr;
        engine = nullptr;
    }

    void runCallback(const float* const* inputs, int numInputs)
    {
        outBuffer.clear();
        device->audioDeviceIOCallbackWithContext(inputs, numInputs,
                                                 outBuffer.getArrayOfWritePointers(),
                                                 outBuffer.getNumChannels(),
                                                 kBlockSize, {});
    }

    float outputLevel() const
    {
        return std::max(outBuffer.getMagnitude(0, 0, kBlockSize),
                        outBuffer.getMagnitude(1, 0, kBlockSize));
    }

    std::shared_ptr<AudiumEngine> engine;
    std::shared_ptr<AudioBusInterface> audioBusInterface;
    std::shared_ptr<LinkAudioDevice> device;
    AudioBuffer<float> inBuffer;
    AudioBuffer<float> outBuffer;
};
} // namespace

SCENARIO("switching to an output-only device", "[engine][link][routing]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock(Thread::getCurrentThread());

    GIVEN("a monitoring channel fed by a 2-input device")
    {
        DeviceHarness harness;

        FakeDevice twoInputs(2);
        harness.device->audioDeviceAboutToStart(&twoInputs);
        harness.runCallback(harness.inBuffer.getArrayOfReadPointers(), 2);

        REQUIRE(harness.audioBusInterface->getChannelLevel(0) == Catch::Approx(kInputValue));
        REQUIRE(harness.outputLevel() > 0.f);

        WHEN("the device changes to one without inputs")
        {
            FakeDevice noInputs(0);
            harness.device->audioDeviceAboutToStart(&noInputs);
            harness.runCallback(nullptr, 0);

            THEN("the monitored channel and the output fall silent instead of "
                 "replaying the previous device's last block")
            {
                REQUIRE(harness.audioBusInterface->getChannelLevel(0) == Catch::Approx(0.f));
                REQUIRE(harness.outputLevel() == Catch::Approx(0.f));
            }
        }
    }
}
