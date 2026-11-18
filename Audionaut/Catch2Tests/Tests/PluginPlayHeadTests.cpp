//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cmath>
#include <memory>

#include "TestUtils.h"
#include "LiveRenderHarness.h"
#include "TestPluginInstance.h"

#include "Engine/Analysis/AnalysisWorker.h"
#include "Engine/Effects/EffectRegistry.h"
#include "Engine/Effects/Hosting/HostedEffect.h"
#include "Engine/Effects/Hosting/PluginTypeId.h"
#include "Engine/Effects/MixerPlanner.h"
#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/PlayList/TransportLoop.h"
#include "Engine/Project/ProjectSerializer.h"
#include "Engine/Provider/TempoProvider.h"
#include "Engine/Resource/AudioResourceContainer.h"

using namespace audium;
using namespace audium::test;
using Catch::Approx;

namespace
{
constexpr double kSampleRate = 44100.0;
constexpr int kBlockSize = 512;
constexpr double kTempo = 90.0;
const std::string kTypeId = "test:playhead";

/// The message manager the engine and the planner's updater need, for one scenario.
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

/**
 * An engine with one track, a four second clip on it and a test plugin as
 * the track's insert: the plugin is created as the plugin host creates one,
 * with the render transport of the registry it is created from, and notes
 * what its playhead says in every block.
 */
struct Session
{
    Session()
    {
        engine = AudiumFactory::createAudiumEngine();
        engine->getAudioResourceContainer()->getAnalysisWorker()->setAutoAnalysisEnabled (false);
        engine->getProjectSerializer()->createNewProject (0);
        engine->getPlayListScheduler()->getTempoProvider()->setTempo (kTempo);

        auto container = engine->getAudioTrackContainer();
        REQUIRE (container->deleteAudioTrack (container->getAudioTrack (0)));
        const auto file = createSineAudioFile (440.0, 4.0, 0.0, kSampleRate);
        REQUIRE (container->addAudioFiles ({ file.getFullPathName() }, 0.0, [] (std::string) {}, false));
        REQUIRE (container->getNumItems() == 1);

        auto& registry = container->getMixerPlanner()->getRegistry();
        registry.add ({ kTypeId, "Playhead Test" }, [this, &registry]
        {
            auto instance = std::make_unique<TestPluginInstance>();
            plugin = instance.get();
            const auto description = TestPluginInstance::describe ({});
            auto effect = std::make_unique<HostedEffect> (PluginTypeId::typeInfo (description), description, std::move (instance));
            effect->setRenderTransport (registry.getRenderTransport());
            return effect;
        });
        REQUIRE (container->getAudioTrack (0)->getTrackInserts().add (kTypeId) != nullptr);
        REQUIRE (plugin != nullptr);

        engine->getPlayListScheduler()->commitPlayListData();
    }

    std::shared_ptr<AudiumEngine> engine;
    TestPluginInstance* plugin = nullptr;   // owned by the insert slot's effect
};

double beatsAt (double seconds) { return seconds * kTempo / 60.0; }
} // namespace

SCENARIO ("a plugin in a bounce is told the position of the block being rendered", "[engine][effects][hosting][playhead]")
{
    MessageThread messageThread;

    GIVEN ("a project at 90 bpm with a plugin on its track")
    {
        Session session;
        auto& positions = session.plugin->positions;

        WHEN ("half a second is bounced from the start")
        {
            bounceProject (*session.engine, 0.0, 0.5, kBlockSize, kSampleRate, 2);

            THEN ("every block is reported as playing, a block further on, at the project's tempo")
            {
                REQUIRE (positions.size() >= 43);   // 22050 samples in blocks of 512
                for (size_t i = 0; i < positions.size(); ++i)
                {
                    CAPTURE (i);
                    const auto samples = static_cast<juce::int64> (i) * kBlockSize;
                    REQUIRE (positions[i].getIsPlaying());
                    REQUIRE (positions[i].getTimeInSamples() == juce::Optional<juce::int64> (samples));
                    REQUIRE (*positions[i].getTimeInSeconds() == Approx (samples / kSampleRate).margin (1.0e-9));
                    REQUIRE (*positions[i].getPpqPosition() == Approx (beatsAt (samples / kSampleRate)).margin (1.0e-9));
                    REQUIRE (*positions[i].getBpm() == Approx (kTempo));
                    REQUIRE_FALSE (positions[i].getIsLooping());
                }
            }
        }

        WHEN ("the bounce starts a second into the project, the transport standing at its start")
        {
            session.engine->getPlayListScheduler()->setAbsoluteStartPosition (0.0, audium::seconds);
            bounceProject (*session.engine, 1.0, 0.25, kBlockSize, kSampleRate, 2);

            THEN ("the plugin is told the bounce's position, not the session's")
            {
                REQUIRE_FALSE (positions.empty());
                REQUIRE (positions.front().getIsPlaying());
                REQUIRE (positions.front().getTimeInSamples() == juce::Optional<juce::int64> (44100));
                REQUIRE (*positions.front().getPpqPosition() == Approx (beatsAt (1.0)).margin (1.0e-9));
                REQUIRE (positions[1].getTimeInSamples() == juce::Optional<juce::int64> (44100 + kBlockSize));
            }
        }
    }
}

SCENARIO ("a plugin in playback is told whether the transport runs, where and how fast", "[engine][effects][hosting][playhead]")
{
    MessageThread messageThread;

    GIVEN ("a project at 90 bpm with a plugin on its track, behind the device callback")
    {
        Session session;
        auto& positions = session.plugin->positions;
        FakeAudioIODevice device (kSampleRate, kBlockSize, 2, 2);
        LiveBlockDriver driver (*session.engine, device);

        WHEN ("the device runs while the transport stands a second into the project")
        {
            session.engine->getPlayListScheduler()->setAbsoluteStartPosition (1.0, audium::seconds);
            positions.clear();
            driver.idle (4);

            THEN ("the plugin is told so: not playing, at that position and the project's tempo")
            {
                REQUIRE (positions.size() == 4);
                REQUIRE_FALSE (positions.back().getIsPlaying());
                REQUIRE (*positions.back().getTimeInSeconds() == Approx (1.0).margin (1.0e-6));
                REQUIRE (*positions.back().getPpqPosition() == Approx (beatsAt (1.0)).margin (1.0e-6));
                REQUIRE (*positions.back().getBpm() == Approx (kTempo));
            }
        }

        WHEN ("the transport plays from the start")
        {
            driver.play (0.0);
            positions.clear();
            driver.render (0.25);
            REQUIRE (driver.lastIsPlaying());

            THEN ("the blocks are reported as playing and the position moves on")
            {
                REQUIRE (positions.size() >= 2);
                REQUIRE (positions.back().getIsPlaying());
                REQUIRE (*positions.back().getBpm() == Approx (kTempo));
                REQUIRE (*positions.back().getTimeInSeconds() > *positions.front().getTimeInSeconds());
                REQUIRE (*positions.back().getTimeInSeconds() < 1.0);
            }

            AND_WHEN ("it is stopped")
            {
                driver.stop();
                positions.clear();
                driver.idle (4);

                THEN ("the plugin hears that it stands")
                {
                    REQUIRE_FALSE (positions.empty());
                    REQUIRE_FALSE (positions.back().getIsPlaying());
                }
            }
        }

        WHEN ("the transport plays inside an active loop")
        {
            auto loop = session.engine->getPlayListScheduler()->getTransportLoop();
            loop->setLoopPositionRange (session.engine->getAudioTrackContainer(), { 1.0, 3.0 }, audium::seconds);
            loop->setLoopActive (true);
            driver.play (1.5);
            positions.clear();
            driver.render (0.1);

            THEN ("the plugin is told the loop in quarter notes")
            {
                REQUIRE_FALSE (positions.empty());
                REQUIRE (positions.back().getIsLooping());
                REQUIRE (positions.back().getLoopPoints().hasValue());
                REQUIRE (positions.back().getLoopPoints()->ppqStart == Approx (beatsAt (1.0)).margin (1.0e-6));
                REQUIRE (positions.back().getLoopPoints()->ppqEnd == Approx (beatsAt (3.0)).margin (1.0e-6));
            }
        }
    }
}
