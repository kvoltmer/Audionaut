//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>
#include <memory>
#include <vector>

#include "TestUtils.h"
#include "LiveRenderHarness.h"

#include "Engine/Analysis/AnalysisWorker.h"
#include "Engine/Channel/AudioChannel.h"
#include "Engine/Effects/AuxBusContainer.h"
#include "Engine/Effects/BuiltIn/DelayEffect.h"
#include "Engine/Effects/BuiltIn/TrimEffect.h"
#include "Engine/Effects/MixerPlanner.h"
#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/Playback/AudioBusInterface.h"
#include "Engine/Project/ProjectSerializer.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Undo/UndoableEdit.h"
#include "Engine/Undo/UndoableSendAction.h"

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
constexpr int kSettled = 2 * kBlockSize;   // past the send and return gain ramps
const float kMinus6 = juce::Decibels::decibelsToGain (-6.0f);

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
        auto file = dir.getChildFile ("send-sine-" + juce::String (f) + ".wav");
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
 * out 3. A send from channel 0 therefore lands on the aux's left side only
 * (the send follows the pan), and the aux return adds to output 0.
 */
struct Session
{
    explicit Session (int numTracks = 1)
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
    }

    AudioTrackContainer& tracks() { return *engine->getAudioTrackContainer(); }
    AuxBusContainer& auxes() { return tracks().getAuxBuses(); }
    std::shared_ptr<AudioTrack> track (int index) { return tracks().getAudioTrack (index); }
    std::shared_ptr<AudioChannel> channel (int track_, int channel_) { return track (track_)->getChannel (channel_); }
    MixerPlanner& planner() { return *tracks().getMixerPlanner(); }

    /// A send from the channel into the aux, set up in one plan.
    std::shared_ptr<SendState> send (int track_, int channel_, int auxId, float level, bool preFader = false)
    {
        MixerPlanner::ScopedBatch batch (&planner());
        auto state = channel (track_, channel_)->sends.getOrAdd (auxId);
        REQUIRE (state != nullptr);
        state->level.store (level);
        state->preFader.store (preFader);
        return state;
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
    driver.play (0.0);
    auto audio = driver.render (kRenderSeconds);
    REQUIRE (driver.lastIsPlaying());
    driver.stop();
    return audio;
}

/// `scaled` on `channel` is `reference` on `referenceChannel` (the same channel by default) times `factor`, from `from` on.
void requireScaled (const juce::AudioBuffer<float>& scaled, const juce::AudioBuffer<float>& reference,
                    int channel, float factor, int referenceChannel = -1, float tolerance = 2.0e-4f, int from = kSettled)
{
    if (referenceChannel < 0)
        referenceChannel = channel;
    REQUIRE (reference.getMagnitude (referenceChannel, from, reference.getNumSamples() - from) > 0.05f);
    auto maxDiff = 0.0f;
    for (auto i = from; i < scaled.getNumSamples(); ++i)
        maxDiff = std::max (maxDiff, std::abs (scaled.getSample (channel, i) - factor * reference.getSample (referenceChannel, i)));
    CAPTURE (channel, referenceChannel, factor, maxDiff);
    REQUIRE (maxDiff <= tolerance);
}

/// The main mix (outputs 0 and 1) of a live render, to compare with a stereo bounce.
juce::AudioBuffer<float> mainOf (const juce::AudioBuffer<float>& live)
{
    juce::AudioBuffer<float> main (2, live.getNumSamples());
    for (auto c = 0; c < 2; ++c)
        main.copyFrom (c, 0, live, c, 0, live.getNumSamples());
    return main;
}

void requireSilent (const juce::AudioBuffer<float>& audio, int channel, int from = kSettled)
{
    CAPTURE (channel);
    REQUIRE (audio.getMagnitude (channel, from, audio.getNumSamples() - from) <= 1.0e-5f);
}

void requireIdentical (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
{
    const auto diff = compareBuffers (a, b, 0.0f);
    CAPTURE (diff.maxAbs, diff.atSample);
    REQUIRE (diff.firstOverTolerance == -1);
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

SCENARIO ("a project has two aux buses from the start", "[engine][effects][sends]")
{
    MessageThread messageThread;

    GIVEN ("a new project")
    {
        Session session;

        THEN ("Aux 1 and Aux 2 are there, in the plan, and the mix is the one without them")
        {
            REQUIRE (session.auxes().size() == 2);
            REQUIRE (session.auxes().getAt (0)->getId() == 1);
            REQUIRE (session.auxes().getAt (0)->getName() == "Aux 1");
            REQUIRE (session.auxes().getAt (1)->getId() == 2);
            REQUIRE (session.auxes().getAt (1)->getName() == "Aux 2");
            REQUIRE (session.channel (0, 0)->sends.isEmpty());

            const auto* plan = session.planner().getPlans().getCurrent();
            REQUIRE (plan != nullptr);
            REQUIRE (plan->auxes.size() == 2);
            REQUIRE (plan->numSendPairs == 0);
            REQUIRE_FALSE (plan->hasWork());   // nothing sends, no insert: the renderer skips the plan

            // the aux returns are silent, so the main mix is the dry sum
            const auto live = renderLive (*session.engine);
            const auto bounced = bounceProject (*session.engine, 0.0, kRenderSeconds, kBlockSize, kSampleRate, 2);
            requireIdentical (mainOf (live), bounced);
            requireScaled (live, live, 0, 1.0f);
            requireSilent (live, 1);
        }

        WHEN ("a third bus is added and the first removed")
        {
            auto third = session.auxes().add ("Verb");
            REQUIRE (session.tracks().removeAuxBus (1));

            THEN ("the ids are stable and the next bus takes a fresh one")
            {
                REQUIRE (session.auxes().size() == 2);
                REQUIRE (third->getId() == 3);
                REQUIRE (third->getName() == "Verb");
                REQUIRE (session.auxes().getAt (0)->getId() == 2);
                REQUIRE (session.auxes().get (1) == nullptr);
                REQUIRE (session.auxes().add()->getId() == 4);
                REQUIRE (session.auxes().get (4)->getName() == "Aux 4");
            }
        }
    }
}

SCENARIO ("a send feeds its aux bus and the return joins the main mix", "[engine][effects][sends]")
{
    MessageThread messageThread;

    GIVEN ("a session and the same project without sends")
    {
        Session plain;
        const auto reference = renderLive (*plain.engine);
        Session session;

        WHEN ("channel 0 sends into Aux 1 at half level, post-fader")
        {
            session.send (0, 0, 1, 0.5f);

            THEN ("output 0 carries the dry channel plus half of it through the return, output 1 nothing (the send follows the pan)")
            {
                const auto live = renderLive (*session.engine);
                requireScaled (live, reference, 0, 1.5f);
                requireSilent (live, 1);
                requireScaled (live, reference, 3, 1.0f);   // the direct out is untouched
            }

            THEN ("the bounce is the same as the live render, for any number of workers")
            {
                const auto serial = renderLive (*session.engine, 0);
                const auto pooled = renderLive (*session.engine, 3);
                const auto bounced = bounceProject (*session.engine, 0.0, kRenderSeconds, kBlockSize, kSampleRate, 2);
                requireIdentical (serial, pooled);
                requireIdentical (mainOf (serial), bounced);
                requireScaled (bounced, reference, 0, 1.5f);
            }

            THEN ("a mono bounce carries the return as well: the channel unpanned, plus half of it")
            {
                const auto plainMono = bounceProject (*plain.engine, 0.0, kRenderSeconds, kBlockSize, kSampleRate, 1);
                const auto mono = bounceProject (*session.engine, 0.0, kRenderSeconds, kBlockSize, kSampleRate, 1);
                REQUIRE (mono.getNumChannels() == 1);
                requireScaled (mono, plainMono, 0, 1.5f);
            }

            AND_WHEN ("the aux return is at half gain")
            {
                session.auxes().get (1)->setGain (0.5f);
                THEN ("the return contributes a quarter")
                {
                    requireScaled (renderLive (*session.engine), reference, 0, 1.25f);
                }
            }

            AND_WHEN ("the aux return is balanced hard right")
            {
                session.auxes().get (1)->setBalance (1.0f);
                THEN ("the return's left side is turned down: output 0 is dry, and nothing was on the right to begin with")
                {
                    const auto live = renderLive (*session.engine);
                    requireScaled (live, reference, 0, 1.0f);
                    requireSilent (live, 1);
                }

                THEN ("a mono bounce has no side to balance to: the return stays whole")
                {
                    const auto plainMono = bounceProject (*plain.engine, 0.0, kRenderSeconds, kBlockSize, kSampleRate, 1);
                    const auto mono = bounceProject (*session.engine, 0.0, kRenderSeconds, kBlockSize, kSampleRate, 1);
                    requireScaled (mono, plainMono, 0, 1.5f);
                }
            }

            AND_WHEN ("the channel is panned to the centre and the aux return balanced half right")
            {
                session.track (0)->setPan (0.0f, 0);
                session.auxes().get (1)->setBalance (0.5f);
                THEN ("the dry channel is on both sides, the return at half on the left and whole on the right")
                {
                    const auto live = renderLive (*session.engine);
                    Session centred;
                    centred.track (0)->setPan (0.0f, 0);
                    const auto dry = renderLive (*centred.engine);
                    requireScaled (live, dry, 0, 1.0f + 0.5f * 0.5f);
                    requireScaled (live, dry, 1, 1.0f + 0.5f);
                }
            }

            AND_WHEN ("the aux is muted")
            {
                session.auxes().get (1)->setMute (true);
                THEN ("only the dry channel is left")
                {
                    requireScaled (renderLive (*session.engine), reference, 0, 1.0f);
                }
            }

            AND_WHEN ("the send is muted")
            {
                session.channel (0, 0)->sends.get (1)->mute.store (true);
                THEN ("only the dry channel is left")
                {
                    requireScaled (renderLive (*session.engine), reference, 0, 1.0f);
                }
            }

            AND_WHEN ("Aux 1 is soloed")
            {
                session.auxes().get (1)->setSolo (true);
                THEN ("the dry mix gives way: output 0 is the return alone")
                {
                    requireScaled (renderLive (*session.engine), reference, 0, 0.5f);
                }

                THEN ("in a mono bounce as well")
                {
                    const auto plainMono = bounceProject (*plain.engine, 0.0, kRenderSeconds, kBlockSize, kSampleRate, 1);
                    requireScaled (bounceProject (*session.engine, 0.0, kRenderSeconds, kBlockSize, kSampleRate, 1), plainMono, 0, 0.5f);
                }
            }

            AND_WHEN ("Aux 2, which nothing feeds, is soloed")
            {
                session.auxes().get (2)->setSolo (true);
                THEN ("the main mix is silent")
                {
                    requireSilent (renderLive (*session.engine), 0);
                }
            }

            AND_WHEN ("a -6 dB trim sits on the aux")
            {
                auto slot = session.auxes().get (1)->getInserts().add (TrimEffect::typeInfo().id);
                REQUIRE (slot != nullptr);
                slot->getEffect().setParameter (TrimEffect::gainDb, -6.0f);
                THEN ("the return is trimmed, the dry channel not")
                {
                    requireScaled (renderLive (*session.engine), reference, 0, 1.0f + 0.5f * kMinus6);
                }
            }

            AND_WHEN ("a fully wet 100 ms delay without feedback sits on the aux")
            {
                auto slot = session.auxes().get (1)->getInserts().add (DelayEffect::typeInfo().id);
                REQUIRE (slot != nullptr);
                slot->getEffect().setParameter (DelayEffect::timeMs, 100.0f);
                slot->getEffect().setParameter (DelayEffect::feedback, 0.0f);
                slot->getEffect().setParameter (DelayEffect::mix, 1.0f);
                const auto period = static_cast<int> (0.1 * kSampleRate);   // 4410 samples

                THEN ("output 0 is the dry channel plus half of it 100 ms later, output 1 stays silent, the bounce agrees")
                {
                    const auto live = renderLive (*session.engine);
                    // the glide from the default time has long settled by two periods in
                    auto maxDiff = 0.0f;
                    for (auto i = 2 * period; i < live.getNumSamples(); ++i)
                    {
                        const auto expected = reference.getSample (0, i) + 0.5f * reference.getSample (0, i - period);
                        maxDiff = std::max (maxDiff, std::abs (live.getSample (0, i) - expected));
                    }
                    REQUIRE (maxDiff <= 2.0e-4f);
                    requireSilent (live, 1);
                    requireIdentical (mainOf (live), bounceProject (*session.engine, 0.0, kRenderSeconds, kBlockSize, kSampleRate, 2));
                }
            }

            AND_WHEN ("the aux bus is removed")
            {
                REQUIRE (session.tracks().removeAuxBus (1));
                THEN ("the send went with it and the mix is dry")
                {
                    REQUIRE (session.channel (0, 0)->sends.isEmpty());
                    requireScaled (renderLive (*session.engine), reference, 0, 1.0f);
                }
            }
        }

        WHEN ("channel 0 is turned down and sends into Aux 1 pre-fader")
        {
            session.channel (0, 0)->setGain (0.0f);
            session.send (0, 0, 1, 0.5f, true);

            THEN ("the return carries half the channel although the fader is down")
            {
                const auto live = renderLive (*session.engine);
                requireScaled (live, reference, 0, 0.5f);
                requireSilent (live, 1);
            }

            THEN ("so does a mono bounce")
            {
                const auto plainMono = bounceProject (*plain.engine, 0.0, kRenderSeconds, kBlockSize, kSampleRate, 1);
                const auto mono = bounceProject (*session.engine, 0.0, kRenderSeconds, kBlockSize, kSampleRate, 1);
                requireScaled (mono, plainMono, 0, 0.5f);
            }

            AND_WHEN ("the send is switched to post-fader")
            {
                session.channel (0, 0)->sends.get (1)->preFader.store (false);
                THEN ("nothing is left")
                {
                    requireSilent (renderLive (*session.engine), 0);
                }
            }
        }

        WHEN ("channel 0 sends into both buses")
        {
            session.send (0, 0, 1, 0.5f);
            session.send (0, 0, 2, 0.25f);

            THEN ("both returns add up, in strip order, the same for any number of workers")
            {
                const auto serial = renderLive (*session.engine, 0);
                requireScaled (serial, reference, 0, 1.75f);
                requireIdentical (serial, renderLive (*session.engine, 3));
            }
        }
    }
}

SCENARIO ("sends and aux buses save, reopen and undo", "[engine][effects][sends][project][undo]")
{
    MessageThread messageThread;

    GIVEN ("a session with a send and a renamed, trimmed aux")
    {
        Session session;
        session.send (0, 0, 1, 0.5f, true);
        session.auxes().get (1)->setName ("Room");
        session.auxes().get (1)->setGain (0.8f);
        session.auxes().get (1)->getInserts().add (TrimEffect::typeInfo().id);

        WHEN ("the container is written and read back into the same session")
        {
            json written;
            session.tracks().writeToJson (written);
            const auto sendState = session.channel (0, 0)->sends.get (1);
            const auto bus = session.auxes().get (1);
            const auto slot = bus->getInserts().getSlot (0);
            const auto rebuilds = session.planner().getNumRebuilds();
            session.tracks().readFromJson (written, false);

            THEN ("the JSON holds both buses and the send, and the instances stayed")
            {
                REQUIRE (written["aux_buses"].size() == 2);
                REQUIRE (written["aux_buses"][0]["id"] == 1);
                REQUIRE (written["aux_buses"][0]["name"] == "Room");
                REQUIRE (written["aux_buses"][0]["gain"].get<float>() == Approx (0.8f));
                REQUIRE (written["aux_buses"][0]["inserts"].size() == 1);
                REQUIRE (written["aux_buses"][1]["name"] == "Aux 2");
                const auto& sends = written["audio_tracks"][0]["channels"][0]["sends"];
                REQUIRE (sends.size() == 1);
                REQUIRE (sends[0]["aux"] == 1);
                REQUIRE (sends[0]["level"].get<float>() == Approx (0.5f));
                REQUIRE (sends[0]["pre_fader"] == true);
                REQUIRE_FALSE (written["audio_tracks"][0]["channels"][1].contains ("sends"));

                REQUIRE (session.channel (0, 0)->sends.get (1) == sendState);
                REQUIRE (session.auxes().get (1) == bus);
                REQUIRE (bus->getInserts().getSlot (0) == slot);
                REQUIRE (session.planner().getNumRebuilds() == rebuilds);
            }
        }

        WHEN ("the JSON is read into a fresh session with a rebuild")
        {
            json written;
            session.tracks().writeToJson (written);
            Session fresh;
            fresh.tracks().readFromJson (written, true);

            THEN ("the buses and the send are back with their values")
            {
                REQUIRE (fresh.auxes().size() == 2);
                REQUIRE (fresh.auxes().get (1)->getName() == "Room");
                REQUIRE (fresh.auxes().get (1)->getGain() == Approx (0.8f));
                REQUIRE (fresh.auxes().get (1)->getInserts().size() == 1);
                auto state = fresh.channel (0, 0)->sends.get (1);
                REQUIRE (state != nullptr);
                REQUIRE (state->level.load() == Approx (0.5f));
                REQUIRE (state->preFader.load());
            }
        }

        WHEN ("a project saved before there were aux buses is read")
        {
            json written;
            session.tracks().writeToJson (written);
            written.erase ("aux_buses");
            written["audio_tracks"][0]["channels"][0].erase ("sends");
            session.tracks().readFromJson (written, true);

            THEN ("it has the two default buses and no sends")
            {
                REQUIRE (session.auxes().size() == 2);
                REQUIRE (session.auxes().get (1)->getName() == "Aux 1");
                REQUIRE (session.auxes().get (2)->getName() == "Aux 2");
                REQUIRE (session.channel (0, 0)->sends.isEmpty());
            }
        }

        WHEN ("a project whose buses were all removed is read")
        {
            json written;
            session.tracks().writeToJson (written);
            written["aux_buses"] = json::array();
            session.tracks().readFromJson (written, true);

            THEN ("it stays without buses; the send into the missing bus is kept for the file but feeds nothing")
            {
                REQUIRE (session.auxes().isEmpty());
                REQUIRE (session.channel (0, 0)->sends.size() == 1);
                REQUIRE (session.planner().getPlans().getCurrent()->numSendPairs == 0);
            }
        }

        WHEN ("the aux bus is removed as an undoable edit and the edit undone")
        {
            auto& container = session.tracks();
            REQUIRE (applyAsUndoableEdit (container, [&] { return container.removeAuxBus (1); }, "Remove Aux Bus", false));
            REQUIRE (session.auxes().size() == 1);
            REQUIRE (session.channel (0, 0)->sends.isEmpty());
            REQUIRE (container.getUndoManager()->undo());

            THEN ("the bus is back with its name, return, insert and the send into it")
            {
                REQUIRE (session.auxes().size() == 2);
                REQUIRE (session.auxes().getAt (0)->getId() == 1);
                REQUIRE (session.auxes().get (1)->getName() == "Room");
                REQUIRE (session.auxes().get (1)->getGain() == Approx (0.8f));
                REQUIRE (session.auxes().get (1)->getInserts().size() == 1);
                auto state = session.channel (0, 0)->sends.get (1);
                REQUIRE (state != nullptr);
                REQUIRE (state->level.load() == Approx (0.5f));
            }
        }

        WHEN ("a send level and an aux return change through their undo actions")
        {
            auto& container = session.tracks();
            auto undoManager = container.getUndoManager();
            const auto state = session.channel (0, 0)->sends.get (1);
            const auto rebuilds = session.planner().getNumRebuilds();

            auto oldSend = UndoableSendAction::Values::of (*state);
            auto newSend = oldSend;
            newSend.level = 0.9f;
            undoManager->beginNewTransaction();
            REQUIRE (undoManager->perform (new UndoableSendAction (container, 0, 0, 1, oldSend, newSend), "Set Send"));

            auto oldAux = UndoableAuxAction::Values::of (*session.auxes().get (1));
            auto newAux = oldAux;
            newAux.mute = true;
            undoManager->beginNewTransaction();
            REQUIRE (undoManager->perform (new UndoableAuxAction (container, 1, oldAux, newAux), "Set Aux"));

            THEN ("the values change and change back without a new plan")
            {
                REQUIRE (state->level.load() == Approx (0.9f));
                REQUIRE (session.auxes().get (1)->getMute());
                REQUIRE (undoManager->undo());
                REQUIRE_FALSE (session.auxes().get (1)->getMute());
                REQUIRE (undoManager->undo());
                REQUIRE (state->level.load() == Approx (0.5f));
                REQUIRE (session.channel (0, 0)->sends.get (1) == state);
                REQUIRE (session.planner().getNumRebuilds() == rebuilds);
            }
        }
    }
}
