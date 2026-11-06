//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <array>
#include <chrono>
#include <cmath>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "TestUtils.h"
#include "LiveRenderHarness.h"

#include "Engine/Analysis/AnalysisWorker.h"
#include "Engine/Channel/AudioChannel.h"
#include "Engine/Effects/AuxBusContainer.h"
#include "Engine/Effects/BuiltIn/DelayEffect.h"
#include "Engine/Effects/BuiltIn/TrimEffect.h"
#include "Engine/Effects/EffectSlot.h"
#include "Engine/Effects/MixerPlanner.h"
#include "Engine/Effects/PdcDelay.h"
#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/Playback/AudioBusInterface.h"
#include "Engine/Project/ProjectSerializer.h"
#include "Engine/Resource/AudioResourceContainer.h"

using namespace audium;
using namespace audium::test;
using Catch::Approx;

namespace
{
constexpr double kSampleRate = 44100.0;
constexpr int kBlockSize = 512;
constexpr int kNumOutputs = 4;         // outputs 0/1 = main, 2/3 = direct outs
constexpr double kFileSeconds = 2.0;
constexpr double kRenderSeconds = 0.5;

/**
 * The ideal latent plugin: its output is its input, `latency` samples
 * later, and it says so. Registered under "test:latency-N", so a chain can
 * be given any latency the scenario needs and the mix it should produce is
 * known exactly - the plain mix, shifted.
 */
class LatencyTestEffect final : public Effect
{
public:
    static std::string typeId (int latency) { return "test:latency-" + std::to_string (latency); }

    explicit LatencyTestEffect (int latency_) :
        Effect ({ typeId (latency_), "Latency " + std::to_string (latency_) }, noParameters),
        latency (latency_)
    {
    }

    void prepare (double, int maxBlockSize, int numChannels) override
    {
        line = std::make_unique<PdcDelay> (numChannels, latency, maxBlockSize);
    }

    void reset() noexcept override
    {
        if (line != nullptr)
            line->reset();
    }

    void process (juce::dsp::AudioBlock<float>& block) noexcept override
    {
        if (line != nullptr)
            line->process (block);
    }

    int getLatencySamples() const noexcept override { return latency; }

private:
    static constexpr std::array<EffectParameterInfo, 0> noParameters {};
    const int latency;
    std::unique_ptr<PdcDelay> line;
};

/// Makes the latencies the scenarios use available to the engine's registry.
void registerLatencyEffects (EffectRegistry& registry)
{
    for (const auto latency : { 200, 256, 300, 480 })
        registry.add ({ LatencyTestEffect::typeId (latency), "Latency" }, [latency] { return std::make_unique<LatencyTestEffect> (latency); });
}

/// One stereo sine file per track, 24-bit, written into `dir`.
std::vector<juce::File> writeSourceFiles (const juce::File& dir, int numTracks)
{
    std::vector<juce::File> files;
    juce::WavAudioFormat wav;
    const auto numSamples = static_cast<int> (kFileSeconds * kSampleRate);
    juce::AudioBuffer<float> buffer (2, numSamples);
    for (auto f = 0; f < numTracks; ++f)
    {
        const auto hz = 330.0 * (f + 1);
        for (auto i = 0; i < numSamples; ++i)
        {
            const auto t = i / kSampleRate;
            buffer.setSample (0, i, 0.3f * static_cast<float> (std::sin (2.0 * juce::MathConstants<double>::pi * hz * t)));
            buffer.setSample (1, i, 0.3f * static_cast<float> (std::sin (2.0 * juce::MathConstants<double>::pi * hz * 1.5 * t)));
        }
        auto file = dir.getChildFile ("latency-sine-" + juce::String (f) + ".wav");
        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());
        REQUIRE (stream != nullptr);
        const auto options = juce::AudioFormatWriterOptions().withSampleRate (kSampleRate)
                                                             .withNumChannels (2)
                                                             .withBitsPerSample (24);
        auto writer = wav.createWriterFor (stream, options);
        REQUIRE (writer != nullptr);
        REQUIRE (writer->writeFromAudioSampleBuffer (buffer, 0, numSamples));
        files.push_back (file);
    }
    return files;
}

/**
 * A fresh engine with stereo tracks, one clip each: every track's channel 0
 * is panned hard left into the main mix, its channel 1 goes to the direct
 * out 3. Output 0 is the sum of the channels 0 (plus any aux return),
 * output 3 the sum of the channels 1. The test latencies are registered.
 */
struct Session
{
    explicit Session (int numTracks = 2)
    {
        engine = AudiumFactory::createAudiumEngine();
        engine->getAudioResourceContainer()->getAnalysisWorker()->setAutoAnalysisEnabled (false);
        engine->getProjectSerializer()->createNewProject (0);
        auto container = engine->getAudioTrackContainer();
        REQUIRE (container->getNumItems() == 1);
        REQUIRE (container->deleteAudioTrack (container->getAudioTrack (0)));
        files = writeSourceFiles (engine->getAudioResourceContainer()->getCurrentAudioFileDirectory(), numTracks);
        for (const auto& file : files)
            REQUIRE (container->addAudioFiles ({ file.getFullPathName() }, 0.0, [] (std::string) {}, false));
        REQUIRE (container->getNumItems() == numTracks);
        for (auto t = 0; t < numTracks; ++t)
        {
            auto track = container->getAudioTrack (t);
            track->setPan (-1.0f, 0);
            track->getChannel (1)->setOutputChannel (3);
        }
        engine->getPlayListScheduler()->commitPlayListData();
        registerLatencyEffects (planner().getRegistry());
    }

    AudioTrackContainer& tracks() { return *engine->getAudioTrackContainer(); }
    AuxBusContainer& auxes() { return tracks().getAuxBuses(); }
    std::shared_ptr<AudioTrack> track (int index) { return tracks().getAudioTrack (index); }
    std::shared_ptr<AudioChannel> channel (int track_, int channel_) { return track (track_)->getChannel (channel_); }
    MixerPlanner& planner() { return *tracks().getMixerPlanner(); }

    /// The plan as a device or bounce at the test format would run it: the slots prepared, the latency worked out.
    const MixPlan& preparedPlan()
    {
        planner().prepare (kSampleRate, kBlockSize);
        return *planner().getPlans().getCurrent();
    }

    /// A latent insert at the end of the chain.
    std::shared_ptr<EffectSlot> addLatency (InsertChain& chain, int latency)
    {
        auto slot = chain.add (LatencyTestEffect::typeId (latency));
        REQUIRE (slot != nullptr);
        return slot;
    }

    /// A send from the channel into the aux, post-fader.
    void send (int track_, int channel_, int auxId, float level)
    {
        MixerPlanner::ScopedBatch batch (&planner());
        auto state = channel (track_, channel_)->sends.getOrAdd (auxId);
        REQUIRE (state != nullptr);
        state->level.store (level);
    }

    std::shared_ptr<AudiumEngine> engine;
    std::vector<juce::File> files;
};

/// Play from the start through the live callback with numWorkers pool threads.
juce::AudioBuffer<float> renderLive (AudiumEngine& engine, int numWorkers = 0)
{
    auto bus = engine.getAudioBusInterface();
    bus->setNumRenderThreads (numWorkers);
    FakeAudioIODevice device (kSampleRate, kBlockSize, 2, kNumOutputs);
    LiveBlockDriver driver (engine, device);
    // a bounce just before leaves its voices fading out in the first live
    // blocks; with the fade in flight through the delay lines, the output
    // is silent only once those have run empty, which here they have
    driver.idle (4);
    driver.play (0.0);
    auto audio = driver.render (kRenderSeconds);
    REQUIRE (driver.lastIsPlaying());
    driver.stop();
    return audio;
}

juce::AudioBuffer<float> bounce (AudiumEngine& engine, int numChannels = 2)
{
    return bounceProject (engine, 0.0, kRenderSeconds, kBlockSize, kSampleRate, numChannels);
}

/// Sample for sample the same, every channel.
void requireIdentical (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    const auto diff = compareBuffers (a, b, 0.0f);
    CAPTURE (diff.maxAbs, diff.atSample, diff.atChannel);
    REQUIRE (diff.firstOverTolerance == -1);
    REQUIRE (diff.referenceMagnitude > 0.05f);   // something was compared
}

/// `late` on `channel` is `reference` on the same channel, `shift` samples later, sample for sample, and silent before that.
void requireShifted (const juce::AudioBuffer<float>& late, const juce::AudioBuffer<float>& reference, int channel, int shift)
{
    REQUIRE (late.getNumSamples() == reference.getNumSamples());
    REQUIRE (reference.getMagnitude (channel, 0, reference.getNumSamples()) > 0.05f);
    CAPTURE (channel, shift);
    REQUIRE (late.getMagnitude (channel, 0, shift) == 0.0f);
    auto maxDiff = 0.0f;
    auto at = -1;
    for (auto i = shift; i < late.getNumSamples(); ++i)
    {
        const auto diff = std::abs (late.getSample (channel, i) - reference.getSample (channel, i - shift));
        if (diff > maxDiff)
        {
            maxDiff = diff;
            at = i;
        }
    }
    CAPTURE (maxDiff, at);
    REQUIRE (maxDiff == 0.0f);
}

/// The message manager the engines and the planner need, for one scenario.
struct MessageThread
{
    MessageThread()
    {
        juce::MessageManager::getInstance();
        lock = std::make_unique<juce::MessageManagerLock> (juce::Thread::getCurrentThread());
    }
    ~MessageThread()
    {
        lock = nullptr;
        juce::DeletedAtShutdown::deleteAll();
        juce::MessageManager::deleteInstance();
    }
    std::unique_ptr<juce::MessageManagerLock> lock;

private:
    JUCE_DECLARE_NON_COPYABLE (MessageThread)
};
} // namespace

SCENARIO ("a PdcDelay delays a block by whole samples, whatever the block size", "[engine][effects][latency]")
{
    GIVEN ("an impulse on two channels, fed through a 100 sample delay in blocks of various lengths")
    {
        constexpr int delay = 100;
        constexpr int maxBlock = 256;
        PdcDelay line (2, delay, maxBlock);
        REQUIRE (line.getDelaySamples() == delay);
        REQUIRE (line.matches (2, delay, maxBlock));
        REQUIRE_FALSE (line.matches (2, delay + 1, maxBlock));

        juce::AudioBuffer<float> signal (2, 1024);
        signal.clear();
        signal.setSample (0, 0, 1.0f);
        signal.setSample (1, 3, -0.5f);
        juce::AudioBuffer<float> output (2, 1024);
        output.clear();

        // shorter than the delay, longer than it, the prepared maximum, odd sizes
        const int blockSizes[] = { 7, 64, 256, 150, 1, 256, 33, 200, 57 };
        auto position = 0;
        auto next = 0;
        while (position < signal.getNumSamples())
        {
            const auto numSamples = std::min (blockSizes[next++ % std::size (blockSizes)], signal.getNumSamples() - position);
            juce::dsp::AudioBlock<float> block (signal.getArrayOfWritePointers(), 2, static_cast<size_t> (position), static_cast<size_t> (numSamples));
            line.process (block);
            position += numSamples;
        }

        THEN ("each impulse comes out exactly `delay` samples later and nothing else does")
        {
            REQUIRE (signal.getSample (0, delay) == 1.0f);
            REQUIRE (signal.getSample (1, delay + 3) == -0.5f);
            signal.setSample (0, delay, 0.0f);
            signal.setSample (1, delay + 3, 0.0f);
            REQUIRE (signal.getMagnitude (0, signal.getNumSamples()) == 0.0f);
        }
    }

    GIVEN ("a delay of zero and a reset one")
    {
        PdcDelay none (1, 0, 64);
        PdcDelay some (1, 10, 64);
        juce::AudioBuffer<float> tone (1, 64);
        for (auto i = 0; i < 64; ++i)
            tone.setSample (0, i, static_cast<float> (i));
        auto copy = tone;

        THEN ("zero passes the block through, and a reset line is silent for its length again")
        {
            juce::dsp::AudioBlock<float> block (tone);
            none.process (block);
            REQUIRE (compareBuffers (tone, copy, 0.0f).firstOverTolerance == -1);

            some.process (block);
            REQUIRE (tone.getMagnitude (0, 0, 10) == 0.0f);
            REQUIRE (tone.getSample (0, 10) == 0.0f);   // copy[0]
            REQUIRE (tone.getSample (0, 11) == 1.0f);
            some.reset();
            tone = copy;
            juce::dsp::AudioBlock<float> again (tone);
            some.process (again);
            REQUIRE (tone.getMagnitude (0, 0, 10) == 0.0f);
            REQUIRE (tone.getSample (0, 11) == 1.0f);
        }
    }
}

SCENARIO ("a latent slot comes back from a bypass aligned, however the toggle is timed", "[engine][effects][latency]")
{
    GIVEN ("a bypassed slot around a pure 1200-sample delay, prepared for 512-sample blocks, fed a ramp")
    {
        constexpr auto latency = 1200;
        constexpr auto block = 512;
        EffectSlot slot (std::make_unique<LatencyTestEffect> (latency));
        slot.setEnabled (false);
        slot.prepare (kSampleRate, block, 1);
        REQUIRE (slot.getLatencySamples() == latency);

        auto fed = 0;
        juce::AudioBuffer<float> buffer (1, block);
        const auto processBlock = [&]
        {
            for (auto i = 0; i < block; ++i)
                buffer.setSample (0, i, static_cast<float> (fed + i));
            juce::dsp::AudioBlock<float> view (buffer);
            slot.process (view);
            // bypassed or running, the slot is the ramp 1200 samples late:
            // the crossfades mix identical signals, the catch-up feeds the dry
            for (auto i = 0; i < block; ++i)
            {
                const auto expected = fed + i - latency;
                REQUIRE (buffer.getSample (0, i) == static_cast<float> (expected < 0 ? 0 : expected));
            }
            fed += block;
        };

        WHEN ("it is switched on, off again while it still catches up, and on once more")
        {
            processBlock();
            slot.setEnabled (true);
            processBlock();             // catching up: 512 of 1200 fed
            slot.setEnabled (false);
            processBlock();             // bypassed before the crossfade began
            slot.setEnabled (true);
            for (auto n = 0; n < 6; ++n)
                processBlock();         // a fresh catch-up of 1200, then the crossfade

            THEN ("every sample came out 1200 late: the second return started afresh")
            {
                REQUIRE (fed == 9 * block);
            }
        }
    }
}

SCENARIO ("a latent insert makes the whole output later and leaves the mix aligned", "[engine][effects][latency]")
{
    MessageThread messageThread;

    GIVEN ("two tracks, rendered plain for reference")
    {
        Session plain;
        const auto referenceLive = renderLive (*plain.engine);
        const auto referenceBounce = bounce (*plain.engine);
        Session session;

        WHEN ("track 0's channel 0 gets an insert with 480 samples of latency")
        {
            session.addLatency (session.channel (0, 0)->inserts, 480);

            THEN ("the plan says 480 at the fader point and nothing after it")
            {
                const auto& plan = session.preparedPlan();
                REQUIRE (plan.latency.preFader == 480);
                REQUIRE (plan.latency.aux == 0);
                REQUIRE (plan.latency.master == 0);
                REQUIRE (plan.latency.atMainOutput() == 480);
                REQUIRE (plan.latency.atStemOutput() == 480);
                REQUIRE (plan.channels[0].preFaderDelay == nullptr);      // the late channel itself waits for nobody
                REQUIRE (plan.channels[1].preFaderDelay->getDelaySamples() == 480);
                REQUIRE (plan.channels[2].preFaderDelay->getDelaySamples() == 480);
                REQUIRE (plan.mainDryDelay == nullptr);
                REQUIRE (session.engine->getAudioBusInterface()->getOutputLatencySamples() == 480);   // adopted at once: no device runs
            }

            THEN ("live, every output is the plain one 480 samples later: the other channels waited, the direct outs too")
            {
                const auto live = renderLive (*session.engine);
                requireShifted (live, referenceLive, 0, 480);
                requireShifted (live, referenceLive, 3, 480);
                REQUIRE (session.engine->getAudioBusInterface()->getOutputLatencySamples() == 480);
            }

            THEN ("three workers render the same, bit for bit")
            {
                requireIdentical (renderLive (*session.engine, 3), renderLive (*session.engine, 0));
            }

            THEN ("the bounce drops the latency and is the plain bounce, sample for sample")
            {
                requireIdentical (bounce (*session.engine), referenceBounce);
            }

            THEN ("a stem export is aligned at the fader point, a mono bounce at the main output")
            {
                requireIdentical (bounce (*session.engine, 4), bounce (*plain.engine, 4));
                requireIdentical (bounce (*session.engine, 1), bounce (*plain.engine, 1));
            }
        }

        WHEN ("latency sits on a track chain and on a channel chain of the other track")
        {
            session.addLatency (session.track (0)->getTrackInserts(), 300);
            session.addLatency (session.channel (0, 0)->inserts, 200);   // track 0, channel 0: 500 in all
            session.addLatency (session.channel (1, 1)->inserts, 480);   // track 1, channel 1: 480

            THEN ("latency adds along a path and the fader point is the longest one")
            {
                const auto& plan = session.preparedPlan();
                REQUIRE (plan.latency.preFader == 500);
                REQUIRE (plan.channels[0].preFaderDelay == nullptr);
                REQUIRE (plan.channels[1].preFaderDelay->getDelaySamples() == 200);   // track 0, channel 1: only the track chain
                REQUIRE (plan.channels[2].preFaderDelay->getDelaySamples() == 500);
                REQUIRE (plan.channels[3].preFaderDelay->getDelaySamples() == 20);
                requireIdentical (bounce (*session.engine), referenceBounce);
                requireShifted (renderLive (*session.engine), referenceLive, 0, 500);
            }
        }

        WHEN ("the latent insert is bypassed")
        {
            auto slot = session.addLatency (session.channel (0, 0)->inserts, 480);
            slot->setEnabled (false);

            THEN ("the chain is as late as with the effect running, so the bounce is still the plain one")
            {
                REQUIRE (session.preparedPlan().latency.preFader == 480);
                requireIdentical (bounce (*session.engine), referenceBounce);
            }

            THEN ("toggling it while playing never moves the signal")
            {
                FakeAudioIODevice device (kSampleRate, kBlockSize, 2, kNumOutputs);
                LiveBlockDriver driver (*session.engine, device);
                driver.play (0.0);
                // rendered in whole blocks, as the driver trims a render to the
                // seconds asked for and would drop the rest of a block
                juce::AudioBuffer<float> live (kNumOutputs, static_cast<int> (kRenderSeconds * kSampleRate));
                const auto third = 14 * kBlockSize;
                auto write = 0;
                const auto append = [&] (const juce::AudioBuffer<float>& part)
                {
                    const auto count = std::min (part.getNumSamples(), live.getNumSamples() - write);
                    for (auto c = 0; c < kNumOutputs; ++c)
                        live.copyFrom (c, write, part, c, 0, count);
                    write += count;
                };
                append (driver.render (third / kSampleRate));
                slot->setEnabled (true);
                append (driver.render (third / kSampleRate));
                slot->setEnabled (false);
                append (driver.render ((live.getNumSamples() - write) / kSampleRate));
                driver.stop();

                // a pure delay bypassed is the same signal as running: the
                // crossfades mix identical signals, and nothing shifts
                requireShifted (live, referenceLive, 0, 480);
            }
        }
    }
}

SCENARIO ("aux and master chains compensate at their own summing points", "[engine][effects][latency][sends]")
{
    MessageThread messageThread;

    GIVEN ("two tracks sending into both aux buses, rendered without latency for reference")
    {
        Session plain;
        plain.send (0, 0, 1, 0.5f);
        plain.send (1, 0, 2, 0.25f);
        const auto referenceBounce = bounce (*plain.engine);
        const auto referenceLive = renderLive (*plain.engine);

        Session session;
        session.send (0, 0, 1, 0.5f);
        session.send (1, 0, 2, 0.25f);

        WHEN ("Aux 1 gets 300 samples of latency")
        {
            session.addLatency (session.auxes().get (1)->getInserts(), 300);

            THEN ("the dry mix and Aux 2's return wait for it, and the bounce is the plain one")
            {
                const auto& plan = session.preparedPlan();
                REQUIRE (plan.latency.preFader == 0);
                REQUIRE (plan.latency.aux == 300);
                REQUIRE (plan.auxes[0].returnDelay == nullptr);
                REQUIRE (plan.auxes[1].returnDelay->getDelaySamples() == 300);
                REQUIRE (plan.mainDryDelay->getDelaySamples() == 300);
                REQUIRE (plan.channels[0].directOutDelay->getDelaySamples() == 300);   // a direct out skips the aux stage and waits for it
                requireIdentical (bounce (*session.engine), referenceBounce);
                const auto live = renderLive (*session.engine);
                requireShifted (live, referenceLive, 0, 300);
                requireShifted (live, referenceLive, 3, 300);
            }
        }

        WHEN ("the master gets 256 samples of latency")
        {
            session.addLatency (session.tracks().getMasterInserts(), 256);

            THEN ("everything is 256 later, the direct outs included, and the bounce is the plain one")
            {
                const auto& plan = session.preparedPlan();
                REQUIRE (plan.latency.master == 256);
                REQUIRE (plan.latency.atMainOutput() == 256);
                REQUIRE (plan.latency.atStemOutput() == 0);   // the stems leave before the master
                requireIdentical (bounce (*session.engine), referenceBounce);
                const auto live = renderLive (*session.engine);
                requireShifted (live, referenceLive, 0, 256);
                requireShifted (live, referenceLive, 3, 256);
            }
        }

        WHEN ("every point has latency: a channel 480, Aux 2 300, the master 256")
        {
            session.addLatency (session.channel (1, 0)->inserts, 480);
            session.addLatency (session.auxes().get (2)->getInserts(), 300);
            session.addLatency (session.tracks().getMasterInserts(), 256);

            THEN ("the figures add up to the output latency and the mix is still the plain one")
            {
                const auto& latency = session.preparedPlan().latency;
                REQUIRE (latency.preFader == 480);
                REQUIRE (latency.aux == 300);
                REQUIRE (latency.master == 256);
                REQUIRE (latency.atMainOutput() == 1036);
                REQUIRE (session.engine->getAudioBusInterface()->getOutputLatencySamples() == 1036);
                requireIdentical (bounce (*session.engine), referenceBounce);
                requireIdentical (bounce (*session.engine, 1), bounce (*plain.engine, 1));
                requireShifted (renderLive (*session.engine), referenceLive, 0, 1036);
                requireIdentical (renderLive (*session.engine, 3), renderLive (*session.engine, 0));
            }
        }
    }
}

SCENARIO ("a rebuild while playing carries the delay lines over", "[engine][effects][latency]")
{
    MessageThread messageThread;

    GIVEN ("a session with a latent insert, playing through the live callback")
    {
        Session plain;
        const auto referenceLive = renderLive (*plain.engine);

        Session session;
        session.addLatency (session.channel (0, 0)->inserts, 480);
        FakeAudioIODevice device (kSampleRate, kBlockSize, 2, kNumOutputs);
        LiveBlockDriver driver (*session.engine, device);
        driver.play (0.0);
        juce::AudioBuffer<float> live (kNumOutputs, static_cast<int> (kRenderSeconds * kSampleRate));
        const auto half = 21 * kBlockSize;   // whole blocks: the driver trims a render to the seconds asked for
        const auto first = driver.render (half / kSampleRate);
        for (auto c = 0; c < kNumOutputs; ++c)
            live.copyFrom (c, 0, first, c, 0, half);
        const auto rebuilds = session.planner().getNumRebuilds();

        WHEN ("track 1 gets a unity trim, which rebuilds the plan")
        {
            auto slot = session.channel (1, 0)->inserts.add (TrimEffect::typeInfo().id);
            REQUIRE (slot != nullptr);
            REQUIRE (session.planner().getNumRebuilds() == rebuilds + 1);
            const auto second = driver.render ((live.getNumSamples() - half) / kSampleRate);
            for (auto c = 0; c < kNumOutputs; ++c)
                live.copyFrom (c, half, second, c, 0, live.getNumSamples() - half);
            driver.stop();

            THEN ("the waiting channels keep their signal in flight: no gap, the whole render is the plain one shifted")
            {
                requireShifted (live, referenceLive, 0, 480);
                requireShifted (live, referenceLive, 3, 480);
            }
        }
    }
}

SCENARIO ("Link and recording take the output latency into account", "[engine][effects][latency][recording]")
{
    MessageThread messageThread;

    GIVEN ("a session with 480 samples of latency on a device with 64 samples in and 128 out")
    {
        Session session;
        session.addLatency (session.channel (0, 0)->inserts, 480);
        FakeAudioIODevice device (kSampleRate, kBlockSize, 2, kNumOutputs);
        device.inputLatency = 64;
        device.outputLatency = 128;
        auto scheduler = session.engine->getPlayListScheduler();
        auto bus = session.engine->getAudioBusInterface();
        LiveBlockDriver driver (*session.engine, device);   // starts the device: latencies read, plan adopted

        THEN ("the beat asked of Link is the one heard: device output latency plus the compensation")
        {
            REQUIRE (bus->getDeviceInputLatencySamples() == 64);
            REQUIRE (bus->getDeviceOutputLatencySamples() == 128);
            REQUIRE (bus->getOutputLatencySamples() == 480);
            // the device's share as LinkEngine keeps it (truncated to the microsecond), the compensation rounded
            using namespace std::chrono;
            const auto deviceShare = duration_cast<microseconds> (duration<double> { 128 / kSampleRate }).count();
            const auto expected = deviceShare + std::llround (480 * 1.0e6 / kSampleRate);
            REQUIRE (session.engine->getLinkAudioDevice()->latencyToOutput().count() == expected);
        }

        WHEN ("a channel is armed and recording starts from one second")
        {
            session.track (0)->setRecordEnabled (0, true);
            driver.idle (1);   // the arm command reaches the renderer
            scheduler->setRecordingArmed (true);
            scheduler->setAbsoluteStartPosition (1.0, audium::seconds);
            scheduler->startRecording();
            scheduler->startPlaying();
            driver.render (0.1);
            driver.stop();

            THEN ("the take is placed the round trip and the compensation before the position it was started at")
            {
                const auto tempo = scheduler->getTempoProvider();
                const auto expectedSeconds = 1.0 - (64 + 128 + 480) / kSampleRate;
                REQUIRE (tempo->clocksToSeconds (scheduler->data.recordingStartPositionClocks) == Approx (expectedSeconds).epsilon (1.0e-9));
                REQUIRE (tempo->clocksToSeconds (bus->getRecordingStartPosition (0)) == Approx (expectedSeconds).epsilon (1.0e-9));
            }
        }

        WHEN ("recording starts at the very beginning")
        {
            session.track (0)->setRecordEnabled (0, true);
            driver.idle (1);
            scheduler->setRecordingArmed (true);
            scheduler->setAbsoluteStartPosition (0.0, audium::seconds);
            scheduler->startRecording();
            scheduler->startPlaying();
            driver.render (0.1);
            driver.stop();

            THEN ("the take cannot go before the timeline and stays at zero")
            {
                REQUIRE (scheduler->data.recordingStartPositionClocks == 0.0);
            }
        }
    }
}

SCENARIO ("a project bounce runs on for the chains' ring-out", "[engine][effects][latency][export]")
{
    MessageThread messageThread;

    GIVEN ("a session whose master has a 100 ms delay without feedback, and the plain one")
    {
        Session plain;
        Session session;
        auto slot = session.tracks().getMasterInserts().add (DelayEffect::typeInfo().id);
        REQUIRE (slot != nullptr);
        slot->getEffect().setParameter (DelayEffect::timeMs, 100.0f);
        slot->getEffect().setParameter (DelayEffect::feedback, 0.0f);
        slot->getEffect().setParameter (DelayEffect::mix, 1.0f);

        const auto bounceToEnd = [] (AudiumEngine& engine, bool includeTails, int numChannels = 2)
        {
            auto config = std::make_shared<ExportAudioConfig>();
            config->fileName = juce::File::createTempFile ("-tail.wav");
            config->sampleRate = kSampleRate;
            config->blockSize = kBlockSize;
            config->numChannels = numChannels;
            config->bitDepth = 32;
            config->includeEffectTails = includeTails;
            REQUIRE (AudioExporter (engine, config).bounce());
            juce::AudioFormatManager formats;
            formats.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (config->fileName));
            REQUIRE (reader != nullptr);
            const auto length = static_cast<int> (reader->lengthInSamples);
            reader.reset();
            config->fileName.deleteFile();
            return length;
        };

        THEN ("the bounce to the project's end is the delay time longer, and as long as the plain one when the tails are left out")
        {
            const auto plainLength = bounceToEnd (*plain.engine, true);
            REQUIRE (plainLength > 0);
            REQUIRE (session.preparedPlan().tailSeconds (false) == Approx (0.1));
            REQUIRE (bounceToEnd (*session.engine, true) - plainLength == Approx (0.1 * kSampleRate).margin (1.0));
            REQUIRE (bounceToEnd (*session.engine, false) == plainLength);
        }

        THEN ("a stem export leaves at the fader point, before the master's tail, and is as long as the plain one")
        {
            REQUIRE (session.preparedPlan().tailSeconds (true) == 0.0);
            REQUIRE (bounceToEnd (*session.engine, true, 4) == bounceToEnd (*plain.engine, true, 4));
        }

        THEN ("a bypassed delay rings out nothing, and neither does a fully dry one")
        {
            const auto plainLength = bounceToEnd (*plain.engine, true);
            slot->setEnabled (false);
            REQUIRE (session.preparedPlan().tailSeconds (false) == 0.0);
            REQUIRE (bounceToEnd (*session.engine, true) == plainLength);
            slot->setEnabled (true);
            slot->getEffect().setParameter (DelayEffect::mix, 0.0f);
            REQUIRE (session.preparedPlan().tailSeconds (false) == 0.0);
        }

        THEN ("a delay time changed since the plan was built counts: the tail is read from the effects as they are")
        {
            const auto plainLength = bounceToEnd (*plain.engine, true);
            REQUIRE (bounceToEnd (*session.engine, true) - plainLength == Approx (0.1 * kSampleRate).margin (1.0));   // the plan is built now
            slot->getEffect().setParameter (DelayEffect::timeMs, 250.0f);   // a value change rebuilds nothing
            REQUIRE (session.preparedPlan().tailSeconds (false) == Approx (0.25));
            REQUIRE (bounceToEnd (*session.engine, true) - plainLength == Approx (0.25 * kSampleRate).margin (1.0));
        }
    }
}
