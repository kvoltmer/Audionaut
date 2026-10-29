//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <memory>
#include <vector>

#include "Engine/Analysis/AnalysisWorker.h"
#include "Engine/Channel/AudioChannel.h"
#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/PlayList/ClipDynamics.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/Playback/AudioBusInterface.h"
#include "Engine/Playback/RenderThreadPool.h"
#include "Engine/Project/ProjectSerializer.h"
#include "Engine/Resource/AudioResourceContainer.h"

#include "LiveRenderHarness.h"

// The live callback renders every track as one job, and the RenderThreadPool
// runs those jobs on worker threads. Each job writes only its own channels and
// its own stereo partial, and the partials are summed in track order on the
// audio thread, so the mix must come out the same for any worker count -
// bit for bit, not within a tolerance. These scenarios render one session
// with 0, 1 and 3 workers and compare; the serial render is also held
// against the offline bounce, which never uses the pool.

using namespace audium;
using namespace audium::test;

namespace {

constexpr double kSampleRate = 44100.0;
constexpr int kBlockSize = 512;
constexpr int kNumTracks = 8;
constexpr int kNumOutputs = 4;         // outputs 0/1 = main, 2/3 = direct outs
constexpr double kFileSeconds = 3.0;
constexpr double kRenderSeconds = 1.0;

/// One stereo sine file per track, distinct pitches, 24-bit, written into `dir`.
std::vector<juce::File> writeSourceFiles (const juce::File& dir)
{
    std::vector<juce::File> files;
    juce::WavAudioFormat wav;
    const auto numSamples = static_cast<int> (kFileSeconds * kSampleRate);
    juce::AudioBuffer<float> buffer (2, numSamples);

    for (auto f = 0; f < kNumTracks; ++f)
    {
        const auto hz = 220.0 * (f + 1);
        for (auto i = 0; i < numSamples; ++i)
        {
            const auto t = i / kSampleRate;
            buffer.setSample (0, i, 0.3f * static_cast<float> (std::sin (2.0 * juce::MathConstants<double>::pi * hz * t)));
            buffer.setSample (1, i, 0.3f * static_cast<float> (std::sin (2.0 * juce::MathConstants<double>::pi * hz * 1.5 * t)));
        }

        auto file = dir.getChildFile ("sine-" + juce::String (f) + ".wav");
        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());
        REQUIRE (stream != nullptr);
        const auto options = juce::AudioFormatWriterOptions().withSampleRate (kSampleRate)
                                                             .withNumChannels (2)
                                                             .withBitsPerSample (24);
        auto writer = wav.createWriterFor (stream, options);   // takes the stream over on success
        REQUIRE (writer != nullptr);
        REQUIRE (writer->writeFromAudioSampleBuffer (buffer, 0, numSamples));
        files.push_back (file);
    }
    return files;
}

/**
 * A fresh engine with kNumTracks stereo tracks, one clip each, and a mix
 * that exercises every stage-C path: pans across the field, channel gain,
 * a muted busChannel, a channel on a direct out, clip gain and fades. With
 * `stretch`, the second half of the tracks is time-stretched (Rubber Band
 * R3), the heavy job the pool exists for.
 *
 * One session per render: a voice that has played once keeps filter and
 * stretcher state, and the comparison is about the worker count, not about
 * a second play.
 */
struct Session
{
    explicit Session (bool stretch)
    {
        engine = AudiumFactory::createAudiumEngine();
        engine->getAudioResourceContainer()->getAnalysisWorker()->setAutoAnalysisEnabled (false);

        engine->getProjectSerializer()->createNewProject (0);
        auto container = engine->getAudioTrackContainer();
        REQUIRE (container->getNumItems() == 1);
        REQUIRE (container->deleteAudioTrack (container->getAudioTrack (0)));

        files = writeSourceFiles (engine->getAudioResourceContainer()->getCurrentAudioFileDirectory());
        for (const auto& file : files)
            REQUIRE (container->addAudioFiles ({ file.getFullPathName() }, 0.0, [] (std::string) {}, false));
        REQUIRE (container->getNumItems() == kNumTracks);

        for (auto t = 0; t < kNumTracks; ++t)
        {
            auto track = container->getAudioTrack (t);
            const auto pan = -1.0f + 2.0f * static_cast<float> (t) / (kNumTracks - 1);
            track->setPan (pan, 0);
            track->setPan (-pan, 1);

            for (const auto& item : track->getPlayListContainer()->getPlayListItems())
            {
                auto& dynamics = item->getDynamics();
                dynamics.setFadeIn (0.05);
                dynamics.setFadeOut (0.05);
                dynamics.setGain (0, 0.8);
                dynamics.setGain (1, 0.7);
                if (stretch && t >= kNumTracks / 2)
                {
                    item->setStretchMode (StretchMode::Stretch);
                    item->setSpeedRatio (0.9);
                }
            }
        }

        container->getAudioTrack (3)->setGain (0.5f, 0);
        container->getAudioTrack (2)->setMute (true, 1);
        container->getAudioTrack (5)->getChannel (1)->setOutputChannel (3);

        engine->getPlayListScheduler()->commitPlayListData();
    }

    std::shared_ptr<AudiumEngine> engine;
    std::vector<juce::File> files;
};

/// Play from the start through the live callback with numWorkers pool threads.
juce::AudioBuffer<float> renderLive (AudiumEngine& engine, int numWorkers)
{
    auto bus = engine.getAudioBusInterface();
    bus->setNumRenderThreads (numWorkers);

    FakeAudioIODevice device (kSampleRate, kBlockSize, 2, kNumOutputs);
    LiveBlockDriver driver (engine, device);
    REQUIRE (bus->getRenderThreadPool().getNumWorkers() == numWorkers);

    driver.play (0.0);
    auto audio = driver.render (kRenderSeconds);
    REQUIRE (driver.lastIsPlaying());
    REQUIRE (bus->getNumTrackJobs() == engine.getAudioTrackContainer()->getNumItems());
    driver.stop();
    return audio;
}

/// The same session rendered with numWorkers, on an engine of its own.
juce::AudioBuffer<float> renderFresh (bool stretch, int numWorkers)
{
    Session session (stretch);
    return renderLive (*session.engine, numWorkers);
}

void requireIdentical (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    const auto diff = compareBuffers (a, b, 0.0f);
    CAPTURE (diff.maxAbs, diff.atSample, diff.atChannel, diff.referenceMagnitude);
    REQUIRE (diff.referenceMagnitude > 0.1f);   // the compared span carries audio
    REQUIRE (diff.maxAbs == 0.0f);
}

bool allFinite (const juce::AudioBuffer<float>& buffer)
{
    for (auto c = 0; c < buffer.getNumChannels(); ++c)
        for (auto i = 0; i < buffer.getNumSamples(); ++i)
            if (! std::isfinite (buffer.getSample (c, i)))
                return false;
    return true;
}

} // namespace

SCENARIO ("the live mix is the same for any number of render workers", "[engine][playback][parallel]")
{
    juce::MessageManager::getInstance();
    juce::MessageManagerLock mmLock (juce::Thread::getCurrentThread());

    GIVEN ("a session of plain tracks with pans, gains, a mute and a direct out")
    {
        const auto serial = renderFresh (false, 0);

        THEN ("one and three workers render it bit for bit")
        {
            requireIdentical (renderFresh (false, 1), serial);
            requireIdentical (renderFresh (false, 3), serial);
        }

        THEN ("the direct out carries audio of its own")
        {
            REQUIRE (serial.getMagnitude (3, 0, serial.getNumSamples()) > 0.05f);
            REQUIRE (serial.getMagnitude (2, 0, serial.getNumSamples()) == 0.0f);
        }

        THEN ("the serial render is what the offline bounce renders")
        {
            Session session (false);
            const auto live = renderLive (*session.engine, 0);
            const auto bounce = bounceProject (*session.engine, 0.0, kRenderSeconds, kBlockSize, kSampleRate, 2);

            juce::AudioBuffer<float> main (2, live.getNumSamples());
            for (auto c = 0; c < 2; ++c)
                main.copyFrom (c, 0, live, c, 0, live.getNumSamples());

            // the bounce waits for its readers instead of playing silence,
            // so the two may differ by float summation order, never more
            const auto diff = compareBuffers (main, bounce, 1.0e-6f);
            CAPTURE (diff.maxAbs, diff.atSample, diff.atChannel, diff.referenceMagnitude);
            REQUIRE (diff.referenceMagnitude > 0.1f);
            REQUIRE (diff.maxAbs <= 1.0e-6f);
        }
    }

    GIVEN ("a session whose second half is time-stretched")
    {
        const auto serial = renderFresh (true, 0);

        THEN ("one and three workers render it bit for bit")
        {
            requireIdentical (renderFresh (true, 1), serial);
            requireIdentical (renderFresh (true, 3), serial);
        }
    }

    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}

SCENARIO ("the render pool follows the device and the session", "[engine][playback][parallel]")
{
    juce::MessageManager::getInstance();
    juce::MessageManagerLock mmLock (juce::Thread::getCurrentThread());

    GIVEN ("a session playing with three workers")
    {
        Session session (false);
        auto& engine = *session.engine;
        auto bus = engine.getAudioBusInterface();
        auto& pool = bus->getRenderThreadPool();
        auto container = engine.getAudioTrackContainer();
        bus->setNumRenderThreads (3);

        WHEN ("a track is added and another removed while it plays")
        {
            FakeAudioIODevice device (kSampleRate, kBlockSize, 2, kNumOutputs);
            LiveBlockDriver driver (engine, device);
            REQUIRE (pool.isRunning());
            REQUIRE (pool.getNumWorkers() == 3);

            driver.play (0.0);
            const auto before = driver.render (0.3);
            REQUIRE (bus->getNumTrackJobs() == kNumTracks);

            REQUIRE (container->addAudioFiles ({ session.files[0].getFullPathName() }, 0.0, [] (std::string) {}, false));
            engine.getPlayListScheduler()->commitPlayListData();
            const auto added = driver.render (0.3);
            REQUIRE (bus->getNumTrackJobs() == kNumTracks + 1);

            REQUIRE (container->deleteAudioTrack (container->getAudioTrack (0)));
            engine.getPlayListScheduler()->commitPlayListData();
            const auto removed = driver.render (0.3);
            REQUIRE (bus->getNumTrackJobs() == kNumTracks);
            REQUIRE (driver.lastIsPlaying());
            driver.stop();

            THEN ("every block rendered, and the audio thread never waited on a lost worker")
            {
                REQUIRE (allFinite (before));
                REQUIRE (allFinite (added));
                REQUIRE (allFinite (removed));
                REQUIRE (removed.getMagnitude (0, 0, removed.getNumSamples()) > 0.1f);
                // plain tracks render in microseconds: a wait of two block
                // periods for a worker would mean the hand-over lost one
                REQUIRE (pool.getStallCount() == 0);
            }
        }

        WHEN ("the device stops and starts again")
        {
            {
                FakeAudioIODevice device (kSampleRate, kBlockSize, 2, kNumOutputs);
                LiveBlockDriver driver (engine, device);
                REQUIRE (pool.getNumWorkers() == 3);
            }
            THEN ("the pool stops with the device and starts with it")
            {
                REQUIRE_FALSE (pool.isRunning());

                bus->setNumRenderThreads (1);
                FakeAudioIODevice device (kSampleRate, kBlockSize, 2, kNumOutputs);
                LiveBlockDriver driver (engine, device);
                REQUIRE (pool.isRunning());
                REQUIRE (pool.getNumWorkers() == 1);
            }
        }

        WHEN ("the thread count is left to the machine")
        {
            bus->setNumRenderThreads (-1);
            THEN ("it resolves to the pool's default, within its limit")
            {
                REQUIRE (bus->resolveNumRenderThreads() == RenderThreadPool::defaultNumWorkers());
                REQUIRE (RenderThreadPool::defaultNumWorkers() >= 0);
                REQUIRE (RenderThreadPool::defaultNumWorkers() <= RenderThreadPool::maxWorkers);
            }
        }
    }

    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}
