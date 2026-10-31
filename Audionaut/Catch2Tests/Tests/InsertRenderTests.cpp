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
#include "Engine/Effects/BuiltIn/CompressorEffect.h"
#include "Engine/Effects/BuiltIn/TrimEffect.h"
#include "Engine/Effects/MixerPlanner.h"
#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/Playback/AudioBusInterface.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Engine/Project/ProjectSerializer.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Undo/UndoableEdit.h"
#include "Engine/Undo/UndoableEffectParameterAction.h"

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
        auto file = dir.getChildFile ("insert-sine-" + juce::String (f) + ".wav");
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
 * out 3 and so never reaches the main mix. Output 0 is therefore the sum
 * of the channels 0, output 3 the sum of the channels 1.
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
    std::shared_ptr<AudioTrack> track (int index) { return tracks().getAudioTrack (index); }
    MixerPlanner& planner() { return *tracks().getMixerPlanner(); }

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

/// `scaled` is `reference` times `factor` on `channel`, from `from` on, within `tolerance`.
void requireScaled (const juce::AudioBuffer<float>& scaled, const juce::AudioBuffer<float>& reference,
                    int channel, float factor, float tolerance = 2.0e-4f, int from = 0)
{
    REQUIRE (reference.getMagnitude (channel, from, reference.getNumSamples() - from) > 0.05f);
    auto maxDiff = 0.0f;
    for (auto i = from; i < scaled.getNumSamples(); ++i)
        maxDiff = std::max (maxDiff, std::abs (scaled.getSample (channel, i) - factor * reference.getSample (channel, i)));
    CAPTURE (channel, factor, maxDiff);
    REQUIRE (maxDiff <= tolerance);
}

void requireIdentical (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b, int from = 0)
{
    const auto diff = compareBuffers (a, b, 0.0f, from);
    CAPTURE (diff.maxAbs, diff.atSample, diff.atChannel, diff.referenceMagnitude);
    REQUIRE (diff.referenceMagnitude > 0.05f);
    REQUIRE (diff.maxAbs == 0.0f);
}

std::shared_ptr<EffectSlot> addTrim (InsertChain& chain, float gainDb)
{
    auto slot = chain.add (TrimEffect::typeInfo().id);
    REQUIRE (slot != nullptr);
    slot->getEffect().setParameter (TrimEffect::gainDb, gainDb);
    return slot;
}

/// The message manager the engines and the planner's updater need, for one scenario.
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

SCENARIO ("inserts render where their chain sits", "[engine][effects][inserts]")
{
    MessageThread messageThread;

    GIVEN ("the session rendered without inserts")
    {
        Session plain;
        const auto reference = renderLive (*plain.engine);
        REQUIRE (reference.getMagnitude (0, 0, reference.getNumSamples()) > 0.1f);
        REQUIRE (reference.getMagnitude (3, 0, reference.getNumSamples()) > 0.1f);
        REQUIRE (reference.getMagnitude (1, 0, reference.getNumSamples()) == 0.0f);

        WHEN ("channel 0 gets a -6 dB trim")
        {
            Session session;
            addTrim (session.track (0)->getChannel (0)->inserts, -6.0f);
            const auto live = renderLive (*session.engine);

            THEN ("the main mix is halved and the direct out of channel 1 is untouched")
            {
                requireScaled (live, reference, 0, kMinus6);
                requireScaled (live, reference, 3, 1.0f, 0.0f);
            }

            THEN ("the offline bounce renders the insert the same way")
            {
                const auto bounce = bounceProject (*session.engine, 0.0, kRenderSeconds, kBlockSize, kSampleRate, 2);
                juce::AudioBuffer<float> main (2, live.getNumSamples());
                for (auto c = 0; c < 2; ++c)
                    main.copyFrom (c, 0, live, c, 0, live.getNumSamples());
                const auto diff = compareBuffers (main, bounce, 1.0e-6f);
                CAPTURE (diff.maxAbs, diff.atSample, diff.atChannel, diff.referenceMagnitude);
                REQUIRE (diff.referenceMagnitude > 0.1f);
                REQUIRE (diff.maxAbs <= 1.0e-6f);
            }
        }

        WHEN ("the track gets a -6 dB trim")
        {
            Session session;
            addTrim (session.track (0)->getTrackInserts(), -6.0f);
            const auto live = renderLive (*session.engine);

            THEN ("both channels are halved: the main mix and the direct out")
            {
                requireScaled (live, reference, 0, kMinus6);
                requireScaled (live, reference, 3, kMinus6);
            }
        }

        WHEN ("the master gets a -6 dB trim")
        {
            Session session;
            addTrim (session.tracks().getMasterInserts(), -6.0f);
            const auto live = renderLive (*session.engine);

            THEN ("the main mix is halved, the direct out is not")
            {
                requireScaled (live, reference, 0, kMinus6);
                requireScaled (live, reference, 3, 1.0f, 0.0f);
            }
        }

        WHEN ("a trim is added but disabled")
        {
            Session session;
            auto slot = addTrim (session.track (0)->getChannel (0)->inserts, -6.0f);
            slot->setEnabled (false);
            const auto live = renderLive (*session.engine);

            THEN ("the render is the plain one, sample for sample")
            {
                requireIdentical (live, reference);
            }
        }
    }
}

SCENARIO ("the live mix with inserts is the same for any number of render workers", "[engine][effects][inserts][parallel]")
{
    MessageThread messageThread;

    GIVEN ("two tracks with channel, track and master inserts")
    {
        const auto render = [] (int numWorkers)
        {
            Session session (2);
            addTrim (session.track (0)->getChannel (0)->inserts, -6.0f);
            addTrim (session.track (1)->getChannel (0)->inserts, -3.0f);
            addTrim (session.track (1)->getTrackInserts(), -2.0f);
            addTrim (session.tracks().getMasterInserts(), -1.0f);
            return renderLive (*session.engine, numWorkers);
        };
        const auto serial = render (0);

        THEN ("three workers render it bit for bit")
        {
            requireIdentical (render (3), serial);
        }
    }
}

SCENARIO ("an insert added or removed while playing takes effect from the next block", "[engine][effects][inserts]")
{
    MessageThread messageThread;

    GIVEN ("a session playing through the live callback")
    {
        Session session;
        FakeAudioIODevice device (kSampleRate, kBlockSize, 2, kNumOutputs);
        LiveBlockDriver driver (*session.engine, device);
        driver.play (0.0);

        const auto level = [] (const juce::AudioBuffer<float>& audio)
        {
            // the last fifth, past the trim's 10 ms ramp
            const auto n = audio.getNumSamples() / 5;
            return audio.getMagnitude (0, audio.getNumSamples() - n, n);
        };

        const auto before = level (driver.render (0.2));
        REQUIRE (before > 0.1f);

        WHEN ("a -12 dB trim is added to channel 0")
        {
            auto& chain = session.track (0)->getChannel (0)->inserts;
            addTrim (chain, -12.0f);
            const auto with = level (driver.render (0.2));

            THEN ("the level drops by 12 dB, and comes back when it is removed")
            {
                REQUIRE (with == Approx (before * juce::Decibels::decibelsToGain (-12.0f)).margin (0.01f));
                REQUIRE (chain.remove (0));
                REQUIRE (level (driver.render (0.2)) == Approx (before).margin (0.01f));
            }
        }

        driver.stop();
    }
}

SCENARIO ("insert edits are undoable and keep their effect instances", "[engine][effects][inserts][undo]")
{
    MessageThread messageThread;

    GIVEN ("a track whose channel 0 got a trim as an undoable edit")
    {
        Session session;
        auto& container = session.tracks();
        auto undoManager = session.engine->getUndoManager();
        auto& chain = session.track (0)->getChannel (0)->inserts;

        json before;
        container.writeToJson (before);

        REQUIRE (applyAsUndoableEdit (container, [&] { return addTrim (chain, -6.0f) != nullptr; }, "Add Trim", false));
        REQUIRE (chain.size() == 1);
        auto slot = chain.getSlot (0);

        json withInsert;
        container.writeToJson (withInsert);
        REQUIRE (withInsert != before);
        REQUIRE (withInsert["audio_tracks"][0]["channels"][0]["inserts"][0]["type"] == "builtin:trim");
        REQUIRE (withInsert["audio_tracks"][0]["channels"][0]["inserts"][0]["state"]["gain_db"].get<float>() == Approx (-6.0f));

        WHEN ("a parameter is changed as its own undo step")
        {
            const auto address = InsertChainAddress::ofChannel (0, 0);
            undoManager->perform (new UndoableEffectParameterAction (container, address, 0, TrimEffect::gainDb, -6.0f, -12.0f), "Trim gain");
            undoManager->beginNewTransaction();
            REQUIRE (slot->getEffect().getParameter (TrimEffect::gainDb) == Approx (-12.0f));

            THEN ("undo restores the value on the same instance, another undo removes the insert, redo brings it back")
            {
                REQUIRE (undoManager->undo());
                REQUIRE (chain.getSlot (0) == slot);
                REQUIRE (slot->getEffect().getParameter (TrimEffect::gainDb) == Approx (-6.0f));

                REQUIRE (undoManager->undo());
                REQUIRE (chain.size() == 0);
                json restored;
                container.writeToJson (restored);
                REQUIRE (restored == before);

                REQUIRE (undoManager->redo());
                REQUIRE (chain.size() == 1);
                json again;
                container.writeToJson (again);
                REQUIRE (again == withInsert);

                REQUIRE (undoManager->redo());
                REQUIRE (chain.getSlot (0)->getEffect().getParameter (TrimEffect::gainDb) == Approx (-12.0f));
            }
        }

        WHEN ("the container state is read back in place")
        {
            json state;
            container.writeToJson (state);
            state["audio_tracks"][0]["channels"][0]["inserts"][0]["state"]["gain_db"] = -9.0;
            REQUIRE (container.restoreFromJson (state, false));

            THEN ("the instance stays and carries the new value")
            {
                REQUIRE (chain.getSlot (0) == slot);
                REQUIRE (slot->getEffect().getParameter (TrimEffect::gainDb) == Approx (-9.0f));
            }
        }
    }
}

SCENARIO ("a project with inserts saves and reopens, one without loads empty chains", "[engine][effects][inserts][project]")
{
    MessageThread messageThread;
    const auto package = testOutputDirectory().getChildFile ("inserts-reopen.audium");
    const auto projectFile = package.getChildFile (ProjectFileStore::projectFileName);

    GIVEN ("a session with a channel, a track and a master insert, saved")
    {
        {
            Session session;
            addTrim (session.track (0)->getChannel (0)->inserts, -6.0f);
            auto trackSlot = addTrim (session.track (0)->getTrackInserts(), -3.0f);
            trackSlot->setEnabled (false);
            addTrim (session.tracks().getMasterInserts(), -1.0f);
            REQUIRE (session.engine->getProjectFileStore()->save (projectFile, nullptr));
        }

        WHEN ("it is opened in a fresh engine")
        {
            auto engine = AudiumFactory::createAudiumEngine();
            engine->getAudioResourceContainer()->getAnalysisWorker()->setAutoAnalysisEnabled (false);
            auto& planner = *engine->getAudioTrackContainer()->getMixerPlanner();
            const auto rebuildsBefore = planner.getNumRebuilds();
            REQUIRE (engine->getProjectFileStore()->open (package, nullptr));
            auto& container = *engine->getAudioTrackContainer();
            REQUIRE (container.getNumItems() == 1);

            THEN ("the load built a plan for the clear and one for the read, not one per channel and chain")
            {
                REQUIRE (planner.getNumRebuilds() - rebuildsBefore <= 2);
                REQUIRE (planner.getPlans().getNumRetired() == 0);
            }

            THEN ("every chain is back with its state and enabled flag")
            {
                auto& channelChain = container.getAudioTrack (0)->getChannel (0)->inserts;
                REQUIRE (channelChain.size() == 1);
                REQUIRE (channelChain.getSlot (0)->getEffect().getParameter (TrimEffect::gainDb) == Approx (-6.0f));
                REQUIRE (channelChain.getSlot (0)->isEnabled());

                auto& trackChain = container.getAudioTrack (0)->getTrackInserts();
                REQUIRE (trackChain.size() == 1);
                REQUIRE (trackChain.getSlot (0)->getEffect().getParameter (TrimEffect::gainDb) == Approx (-3.0f));
                REQUIRE_FALSE (trackChain.getSlot (0)->isEnabled());

                REQUIRE (container.getMasterInserts().size() == 1);
                REQUIRE (container.getMasterInserts().getSlot (0)->getEffect().getParameter (TrimEffect::gainDb) == Approx (-1.0f));
            }

            THEN ("a state without insert keys, read in place, empties the chains")
            {
                json state;
                container.writeToJson (state);
                state.erase ("master_inserts");
                state["audio_tracks"][0].erase ("inserts");
                state["audio_tracks"][0]["channels"][0].erase ("inserts");
                REQUIRE (container.restoreFromJson (state, false));
                REQUIRE (container.getAudioTrack (0)->getChannel (0)->inserts.isEmpty());
                REQUIRE (container.getAudioTrack (0)->getTrackInserts().isEmpty());
                REQUIRE (container.getMasterInserts().isEmpty());

                json written;
                container.writeToJson (written);
                REQUIRE_FALSE (written.contains ("master_inserts"));
                REQUIRE_FALSE (written["audio_tracks"][0].contains ("inserts"));
                REQUIRE_FALSE (written["audio_tracks"][0]["channels"][0].contains ("inserts"));
            }
        }
    }

    package.deleteRecursively();
}

SCENARIO ("a bounce starts its inserts from rest", "[engine][effects][inserts][export]")
{
    MessageThread messageThread;

    const auto addSlowCompressor = [] (Session& session)
    {
        auto slot = session.tracks().getMasterInserts().add (CompressorEffect::typeInfo().id);
        REQUIRE (slot != nullptr);
        slot->getEffect().setParameter (CompressorEffect::thresholdDb, -40.0f);
        slot->getEffect().setParameter (CompressorEffect::ratio, 8.0f);
        slot->getEffect().setParameter (CompressorEffect::releaseMs, 2000.0f);
    };

    GIVEN ("a session with a slow compressor on the master, played live")
    {
        Session session;
        addSlowCompressor (session);
        // leaves the compressor's envelope wherever the playback ended
        renderLive (*session.engine);

        WHEN ("the project is bounced twice")
        {
            const auto first = bounceProject (*session.engine, 0.0, 0.2, kBlockSize, kSampleRate, 2);
            const auto second = bounceProject (*session.engine, 0.0, 0.2, kBlockSize, kSampleRate, 2);

            THEN ("both bounces are the same file, and the one a session that never played renders")
            {
                requireIdentical (first, second);

                Session fresh;
                addSlowCompressor (fresh);
                const auto cold = bounceProject (*fresh.engine, 0.0, 0.2, kBlockSize, kSampleRate, 2);
                const auto diff = compareBuffers (first, cold, 1.0e-6f);
                CAPTURE (diff.maxAbs, diff.atSample);
                REQUIRE (diff.firstOverTolerance == -1);
            }
        }

        WHEN ("the project is bounced with a block eight times the device's")
        {
            auto& planner = session.planner();
            const auto slot = session.tracks().getMasterInserts().getSlot (0);
            const auto blockBefore = planner.getMaxBlockSize();
            const auto bounced = bounceProject (*session.engine, 0.0, 0.2, kBlockSize * 8, kSampleRate, 2);

            THEN ("the compressor is the same instance, prepared for the larger block, and the file the same as at the device's block")
            {
                REQUIRE (session.tracks().getMasterInserts().getSlot (0) == slot);
                REQUIRE (planner.getMaxBlockSize() == kBlockSize * 8);
                REQUIRE (blockBefore == kBlockSize);

                const auto reference = bounceProject (*session.engine, 0.0, 0.2, kBlockSize, kSampleRate, 2);
                const auto diff = compareBuffers (bounced, reference, 1.0e-6f);
                CAPTURE (diff.maxAbs, diff.atSample);
                REQUIRE (diff.firstOverTolerance == -1);
            }
        }

        WHEN ("the project is bounced with a block a quarter of the device's, then played live again")
        {
            auto& planner = session.planner();
            const auto slot = session.tracks().getMasterInserts().getSlot (0);
            REQUIRE (planner.getMaxBlockSize() == kBlockSize);
            const auto bounced = bounceProject (*session.engine, 0.0, 0.2, kBlockSize / 4, kSampleRate, 2);
            const auto blockAfterBounce = planner.getMaxBlockSize();
            renderLive (*session.engine);

            THEN ("the compressor is the same instance, prepared for the smaller block and then for the device's again")
            {
                REQUIRE (session.tracks().getMasterInserts().getSlot (0) == slot);
                REQUIRE (blockAfterBounce == kBlockSize / 4);
                REQUIRE (planner.getMaxBlockSize() == kBlockSize);
                REQUIRE (slot->isPreparedFor (kSampleRate, kBlockSize, 2));
                REQUIRE_FALSE (slot->isPreparedFor (kSampleRate, kBlockSize / 4, 2));

                const auto reference = bounceProject (*session.engine, 0.0, 0.2, kBlockSize, kSampleRate, 2);
                const auto diff = compareBuffers (bounced, reference, 1.0e-6f);
                CAPTURE (diff.maxAbs, diff.atSample);
                REQUIRE (diff.firstOverTolerance == -1);
            }
        }
    }
}

SCENARIO ("the mix plan follows the channel layout", "[engine][effects][inserts]")
{
    MessageThread messageThread;

    GIVEN ("a session with a track insert")
    {
        Session session (2);
        addTrim (session.track (1)->getTrackInserts(), -6.0f);
        auto& planner = session.planner();
        auto& plans = planner.getPlans();
        const auto rebuilds = planner.getNumRebuilds();

        THEN ("the plan lists both tracks, the second with its insert at its first bus channel")
        {
            const auto* plan = plans.getCurrent();
            REQUIRE (plan != nullptr);
            REQUIRE (plan->tracks.size() == 2);
            REQUIRE (plan->channels.size() == 4);
            REQUIRE (plan->tracks[1].firstChannel == 2);
            REQUIRE (plan->tracks[1].numChannels == 2);
            REQUIRE (plan->trackAtChannel[2] == 1);
            REQUIRE (plan->trackAtChannel[1] == -1);
            // no device has prepared a format yet: the slot waits outside the plan
            REQUIRE (plan->tracks[1].inserts.empty());
        }

        WHEN ("a device starts and track 0 is removed")
        {
            FakeAudioIODevice device (kSampleRate, kBlockSize, 2, kNumOutputs);
            LiveBlockDriver driver (*session.engine, device);
            REQUIRE (planner.getNumRebuilds() > rebuilds);
            REQUIRE (plans.getCurrent()->tracks[1].inserts.size() == 1);
            REQUIRE (plans.getAdopted() == plans.getCurrent());

            const auto* before = plans.getCurrent();
            const auto rebuildsBefore = planner.getNumRebuilds();
            REQUIRE (session.tracks().deleteAudioTrack (session.track (0)));

            THEN ("the plan is rebuilt on the spot, once for the whole removal, and the next block takes it over with the layout")
            {
                // no message loop turned: the edit itself built the plan
                REQUIRE (planner.getNumRebuilds() == rebuildsBefore + 1);
                REQUIRE (plans.getCurrent() != before);
                REQUIRE (plans.getCurrent()->tracks.size() == 1);

                // the render thread adopts it with its next block at the
                // latest, behind the channel data of the same edit (here,
                // where the commands are pumped as posted, at once), and the
                // previous plan goes once that happened
                driver.idle (1);
                REQUIRE (plans.getAdopted() == plans.getCurrent());
                plans.releaseRetired();
                REQUIRE (plans.getNumRetired() == 0);
            }

            THEN ("the insert's track moved to bus channel 0 and keeps its instance")
            {
                driver.idle (2);
                const auto* plan = plans.getCurrent();
                REQUIRE (plan->tracks.size() == 1);
                REQUIRE (plan->tracks[0].firstChannel == 0);
                REQUIRE (plan->tracks[0].inserts.size() == 1);
                REQUIRE (plan->tracks[0].inserts[0] == session.track (0)->getTrackInserts().getSlot (0).get());
            }
        }

        WHEN ("no device runs and the layout changes")
        {
            const auto rebuildsBefore = planner.getNumRebuilds();
            REQUIRE (session.track (1)->addChannel() != nullptr);

            THEN ("the plan is built once and taken over at once, so nothing is left retired")
            {
                REQUIRE (planner.getNumRebuilds() == rebuildsBefore + 1);
                REQUIRE (plans.getAdopted() == plans.getCurrent());
                REQUIRE (plans.getNumRetired() == 0);
                REQUIRE (plans.getCurrent()->tracks[1].numChannels == 3);
            }
        }

        WHEN ("a device runs and the track with the insert gets a third channel")
        {
            FakeAudioIODevice device (kSampleRate, kBlockSize, 2, kNumOutputs);
            LiveBlockDriver driver (*session.engine, device);
            const auto slot = session.track (1)->getTrackInserts().getSlot (0);
            REQUIRE (slot->getNumChannels() == 2);
            REQUIRE (slot->isInAPlan());

            const auto rebuildsBefore = planner.getNumRebuilds();
            REQUIRE (session.track (1)->addChannel() != nullptr);
            driver.idle (2);

            THEN ("the insert kept its instance, prepared again for three channels once the plan that ran it was gone")
            {
                // a plan without it, then - the previous plan freed on the
                // spot here, where the commands are pumped as posted - one
                // with it prepared for the new count
                REQUIRE (planner.getNumRebuilds() == rebuildsBefore + 2);
                REQUIRE (session.track (1)->getTrackInserts().getSlot (0) == slot);
                REQUIRE (slot->getNumChannels() == 3);

                const auto* plan = plans.getCurrent();
                REQUIRE (plan->tracks[1].numChannels == 3);
                REQUIRE (plan->tracks[1].inserts.size() == 1);
                REQUIRE (plan->tracks[1].inserts[0] == slot.get());
                REQUIRE (plans.getAdopted() == plan);
            }
        }
    }
}
