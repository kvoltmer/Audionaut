//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <functional>
#include <vector>

#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/PlayList/ClipDynamics.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/PlayList/TransportLoop.h"
#include "Engine/Project/ProjectFileStore.h"

#include "LiveRenderHarness.h"

// The bounce and the live device callback drive the same scheduler, but
// only the bounce had a test. These scenarios render a session through the
// live path (LiveRenderHarness.h) and through bounceProject and compare the
// two sample by sample: plain playback, a start inside a clip, playback
// across loop wraps, the device's block size against the export's, and the
// bounce-then-play sequence a user runs on one engine. A last scenario
// shows the comparison catching a real divergence, so a green run means
// the paths agree and not that nothing was compared.

using namespace audium;
using namespace audium::test;

namespace {

constexpr double kSampleRate = 44100.0;
constexpr int kNumChannels = 2;

/// Both paths run the same per-sample DSP in the same order, so they come
/// out bit-identical today: a measured max |live - bounce| of 0.0 in every
/// case below, at the bounce's block size and at a smaller one, with the
/// clips session resampling 48 kHz material to 44.1 kHz (2026-09-27). The
/// tolerance leaves room for a legitimate change of float summation order
/// somewhere in the bus mix (a few ulp of a full-scale float, ~1e-7 each):
/// 1e-6 is about -120 dBFS, far below anything audible and below the 24-bit
/// export's quantisation step (1.2e-7 * 8). Anything above it is a path
/// diverging (a missed clip start, a wrap a block late, a stale gain), not
/// arithmetic.
constexpr float kTolerance = 1.0e-6f;

const juce::File sessionsDir()
{
    return juce::File (juce::String (CURRENT_SOURCE_DIR)).getChildFile ("TestFiles/Sessions");
}

using Decorate = std::function<void (AudiumEngine&)>;

/// A fresh engine with the session open and decorated.
std::shared_ptr<AudiumEngine> openSession (const juce::String& name, const Decorate& decorate)
{
    auto engine = AudiumFactory::createAudiumEngine();
    const auto session = sessionsDir().getChildFile (name);
    REQUIRE (session.exists());
    REQUIRE (engine->getProjectFileStore()->open (session, nullptr));
    if (decorate)
        decorate (*engine);
    engine->getPlayListScheduler()->commitPlayListData();
    return engine;
}

/// move-channels.audium: one stereo track, three adjacent four-second clips
/// A, B, C at 0, 4 and 8 seconds with per-region gains, hard-panned
/// channels. The session predates clip fades and clip gain, so they are
/// added here: A fades out over its last 0.2 s and its fade reaches 0.04 s
/// past its end (a tail extension into B), B fades in over 0.4 s on a
/// steeper curve and plays at clip gain 0.7 / 0.5, C at 0.8.
const juce::String kClipsSession = "move-channels.audium";
void decorateClips (AudiumEngine& engine)
{
    auto items = engine.getAudioTrackContainer()->getAudioTrack (0)->getPlayListContainer()->getPlayListItems();
    REQUIRE (items.size() == 3);

    auto& a = items[0]->getDynamics();
    a.setFadeOut (0.05);
    a.setFadeOutEnd (-0.01);

    auto& b = items[1]->getDynamics();
    b.setFadeIn (0.1);
    b.setFadeInCurve (2.0);
    b.setGain (0, 0.7);
    b.setGain (1, 0.5);

    auto& c = items[2]->getDynamics();
    c.setGain (0, 0.8);
    c.setGain (1, 0.8);
}

/// simple-sine-loop.audium: a one-second sine at 1.0 s and an active loop
/// over 1.0 - 1.5 s.
const juce::String kLoopSession = "simple-sine-loop.audium";

struct Case
{
    const char* name;
    juce::String session;
    Decorate decorate;
    double startSeconds;
    double lengthSeconds;
    int minLoopWraps;   ///< wraps the live transport must have counted by the end
};

const std::vector<Case>& cases()
{
    static const std::vector<Case> all {
        { "plain playback from the start (A, its fade into B, B's fade-in)", kClipsSession, decorateClips, 0.0, 5.0, 0 },
        { "a start inside a clip, off the block grid",                        kClipsSession, decorateClips, 1.37, 3.3, 0 },
        // 0.5 s before the loop, then 2.5 s inside it: wraps at 1.0, 1.5, 2.0 and 2.5 s of render
        { "playback across the loop wraps",                                   kLoopSession,  nullptr,       0.5, 3.0, 4 },
        // wraps at 0.3, 0.8, 1.3 and 1.8 s of render
        { "a start inside the loop range",                                    kLoopSession,  nullptr,       1.2, 2.0, 4 },
    };
    return all;
}

/// Play, render, Stop - and check the render did what the case says it does.
juce::AudioBuffer<float> renderLive (AudiumEngine& engine, const Case& c, int blockSize)
{
    FakeAudioIODevice device (kSampleRate, blockSize, 2, kNumChannels);
    LiveBlockDriver driver (engine, device);
    driver.play (c.startSeconds);
    auto audio = driver.render (c.lengthSeconds);
    REQUIRE (driver.lastIsPlaying());
    // the loop cases really wrapped (a transport that ran past the loop end
    // would still match a bounce that did the same)
    REQUIRE (engine.getPlayListScheduler()->getTransportLoop()->getLoopCount() >= c.minLoopWraps);
    driver.stop();
    REQUIRE_FALSE (driver.lastIsPlaying());
    return audio;
}

void requireEquivalent (const juce::AudioBuffer<float>& live, const juce::AudioBuffer<float>& bounce,
                        float tolerance = kTolerance)
{
    const auto diff = compareBuffers (live, bounce, tolerance);
    CAPTURE (diff.maxAbs, diff.atSample, diff.atChannel, diff.firstOverTolerance, diff.referenceMagnitude);
    REQUIRE (diff.referenceMagnitude > 0.1f);   // the compared span carries audio
    REQUIRE (diff.maxAbs <= tolerance);
}

} // namespace

SCENARIO ("the live callback renders what the bounce renders", "[engine][live][equivalence]")
{
    juce::MessageManager::getInstance();
    juce::MessageManagerLock mmLock (juce::Thread::getCurrentThread());

    for (const auto& c : cases())
    {
        DYNAMIC_SECTION (c.name)
        {
            constexpr int blockSize = 512;

            GIVEN ("the span bounced offline")
            {
                auto bounceEngine = openSession (c.session, c.decorate);
                const auto bounce = bounceProject (*bounceEngine, c.startSeconds, c.lengthSeconds,
                                                   blockSize, kSampleRate, kNumChannels);
                bounceEngine = nullptr;

                WHEN ("the same span plays through the device callback at the same block size")
                {
                    auto liveEngine = openSession (c.session, c.decorate);
                    const auto live = renderLive (*liveEngine, c, blockSize);
                    liveEngine = nullptr;

                    THEN ("every sample matches")
                    {
                        requireEquivalent (live, bounce);
                    }
                }

                WHEN ("it plays at the smaller block size of a typical device")
                {
                    auto liveEngine = openSession (c.session, c.decorate);
                    const auto live = renderLive (*liveEngine, c, 256);
                    liveEngine = nullptr;

                    THEN ("the block size does not change what is heard")
                    {
                        requireEquivalent (live, bounce);
                    }
                }
            }
        }
    }

    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}

SCENARIO ("bouncing and then playing on one engine renders the same audio", "[engine][live][equivalence]")
{
    juce::MessageManager::getInstance();
    juce::MessageManagerLock mmLock (juce::Thread::getCurrentThread());

    // What a user does: export, then press Play (and the other way round).
    // The exporter bypasses the device, re-prepares the graph at its own
    // block size and puts everything back; nothing of that may leak into
    // the next render.
    for (const auto& c : cases())
    {
        DYNAMIC_SECTION (c.name)
        {
            constexpr int blockSize = 512;

            GIVEN ("one engine with the session open")
            {
                auto engine = openSession (c.session, c.decorate);

                WHEN ("the span is bounced, then played live, then bounced again")
                {
                    const auto firstBounce = bounceProject (*engine, c.startSeconds, c.lengthSeconds,
                                                            blockSize, kSampleRate, kNumChannels);
                    const auto live = renderLive (*engine, c, blockSize);
                    const auto secondBounce = bounceProject (*engine, c.startSeconds, c.lengthSeconds,
                                                             blockSize, kSampleRate, kNumChannels);

                    THEN ("all three renders match")
                    {
                        requireEquivalent (live, firstBounce);
                        requireEquivalent (live, secondBounce);
                    }
                }

                engine = nullptr;
            }
        }
    }

    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}

SCENARIO ("the first device callback after Play renders the bounce's first block", "[engine][live][equivalence]")
{
    juce::MessageManager::getInstance();
    juce::MessageManagerLock mmLock (juce::Thread::getCurrentThread());

    // The real LinkAudioDevice callback, once: Link pins the start beat to
    // that callback's host time, so its block is the one block whose
    // position does not depend on the wall clock (see LiveBlockDriver).
    // This covers the entry point itself - bypass flag, output clearing,
    // the blocks built over the device's channel arrays.
    const auto& c = cases()[1];   // a start inside a clip: audio from sample 0
    constexpr int blockSize = 512;

    GIVEN ("the span bounced offline")
    {
        auto bounceEngine = openSession (c.session, c.decorate);
        const auto bounce = bounceProject (*bounceEngine, c.startSeconds, c.lengthSeconds,
                                           blockSize, kSampleRate, kNumChannels);
        bounceEngine = nullptr;

        WHEN ("Play is pressed and the device runs its first callback")
        {
            auto engine = openSession (c.session, c.decorate);
            juce::AudioBuffer<float> block;
            {
                FakeAudioIODevice device (kSampleRate, blockSize, 2, kNumChannels);
                LiveBlockDriver driver (*engine, device);
                driver.play (c.startSeconds);
                block = driver.deviceCallbackBlock();
                driver.stop();
            }

            THEN ("its output is the bounce's first block")
            {
                juce::AudioBuffer<float> firstBounceBlock (kNumChannels, blockSize);
                for (auto ch = 0; ch < kNumChannels; ++ch)
                    firstBounceBlock.copyFrom (ch, 0, bounce, ch, 0, blockSize);

                requireEquivalent (block, firstBounceBlock);
            }

            engine = nullptr;
        }
    }

    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}

SCENARIO ("the comparison tells a live render that left the loop from the bounce", "[engine][live][equivalence]")
{
    juce::MessageManager::getInstance();
    juce::MessageManagerLock mmLock (juce::Thread::getCurrentThread());

    // The control for the scenarios above: the one-second sine sits at
    // 1.0 s, the loop covers its first half. With the loop off, the live
    // transport plays the sine out and falls silent at 2.0 s (1.5 s into
    // the render) where the looping bounce is still at full scale.
    const auto& c = cases()[2];   // playback across the loop wraps
    constexpr int blockSize = 512;

    GIVEN ("the span bounced offline with the loop active")
    {
        auto bounceEngine = openSession (c.session, c.decorate);
        const auto bounce = bounceProject (*bounceEngine, c.startSeconds, c.lengthSeconds,
                                           blockSize, kSampleRate, kNumChannels);
        bounceEngine = nullptr;

        WHEN ("it plays live with the loop switched off")
        {
            auto engine = openSession (c.session, c.decorate);
            engine->getPlayListScheduler()->getTransportLoop()->setLoopActive (false);
            Case straight = c;
            straight.minLoopWraps = 0;
            const auto live = renderLive (*engine, straight, blockSize);
            engine = nullptr;

            THEN ("the renders agree up to the first wrap and diverge there")
            {
                const auto firstWrap = static_cast<int> (std::llround (1.0 * kSampleRate));   // 1.5 s timeline, 0.5 s in
                const auto sineEnd   = static_cast<int> (std::llround (1.5 * kSampleRate));   // 2.0 s timeline

                const auto before = compareBuffers (live, bounce, kTolerance, 0, firstWrap);
                CAPTURE (before.maxAbs, before.atSample, before.referenceMagnitude);
                REQUIRE (before.referenceMagnitude > 0.9f);
                REQUIRE (before.maxAbs <= kTolerance);

                const auto after = compareBuffers (live, bounce, kTolerance, sineEnd);
                CAPTURE (after.maxAbs, after.atSample, after.firstOverTolerance, after.referenceMagnitude);
                REQUIRE (after.referenceMagnitude > 0.9f);
                REQUIRE (after.maxAbs > 0.9f);
                REQUIRE (live.getMagnitude (sineEnd, live.getNumSamples() - sineEnd) == 0.0f);
            }
        }
    }

    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}
