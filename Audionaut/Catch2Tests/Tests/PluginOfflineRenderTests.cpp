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
#include "Engine/Project/ProjectSerializer.h"
#include "Engine/Resource/AudioResourceContainer.h"

using namespace audium;
using namespace audium::test;
using Catch::Approx;

namespace
{
constexpr double kSampleRate = 44100.0;
constexpr int kBlockSize = 512;
const std::string kTypeId = "test:offline";

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
 * with the render transport of the registry it is created from, and counts
 * the blocks it ran with the non-realtime flag set and without.
 */
struct Session
{
    Session()
    {
        engine = AudiumFactory::createAudiumEngine();
        engine->getAudioResourceContainer()->getAnalysisWorker()->setAutoAnalysisEnabled (false);
        engine->getProjectSerializer()->createNewProject (0);

        auto container = engine->getAudioTrackContainer();
        REQUIRE (container->deleteAudioTrack (container->getAudioTrack (0)));
        const auto file = createSineAudioFile (440.0, 4.0, 0.0, kSampleRate);
        REQUIRE (container->addAudioFiles ({ file.getFullPathName() }, 0.0, [] (std::string) {}, false));
        REQUIRE (container->getNumItems() == 1);

        auto& registry = container->getMixerPlanner()->getRegistry();
        registry.add ({ kTypeId, "Offline Test" }, [this, &registry]
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

} // namespace

SCENARIO ("a plugin is told when it runs in a bounce, and when in playback again", "[engine][effects][hosting][offline]")
{
    MessageThread messageThread;

    GIVEN ("a project with a plugin on its track, behind the device callback")
    {
        Session session;
        auto& plugin = *session.plugin;
        auto& planner = *session.engine->getAudioTrackContainer()->getMixerPlanner();
        FakeAudioIODevice device (kSampleRate, kBlockSize, 2, 2);
        LiveBlockDriver driver (*session.engine, device);

        WHEN ("the transport plays")
        {
            driver.play (0.0);
            driver.render (0.1);
            driver.stop();

            THEN ("the plugin was prepared for live rendering and ran every block that way")
            {
                REQUIRE_FALSE (plugin.preparedOffline);
                REQUIRE (plugin.liveBlocks > 0);
                REQUIRE (plugin.offlineBlocks == 0);
            }
        }

        WHEN ("the project is bounced with the device's block size and rate")
        {
            driver.idle (2);
            const auto liveBefore = plugin.liveBlocks;
            const auto preparesBefore = plugin.prepareCount;
            bounceProject (*session.engine, 0.0, 0.25, kBlockSize, kSampleRate, 2);

            THEN ("the plugin was prepared again, in offline mode, and ran the whole bounce that way")
            {
                REQUIRE (plugin.prepareCount > preparesBefore);
                REQUIRE (plugin.offlineBlocks >= 21);   // 11025 samples in blocks of 512
                REQUIRE (plugin.liveBlocks == liveBefore);
            }

            THEN ("the planner is back to live once the bounce is over")
            {
                REQUIRE_FALSE (planner.isOfflineRendering());
            }

            AND_WHEN ("the device starts again and the transport plays")
            {
                FakeAudioIODevice deviceAfter (kSampleRate, kBlockSize, 2, 2);
                LiveBlockDriver driverAfter (*session.engine, deviceAfter);
                const auto offlineAfterBounce = plugin.offlineBlocks;
                driverAfter.play (0.0);
                driverAfter.render (0.1);
                driverAfter.stop();

                THEN ("the plugin is prepared for live rendering and runs live")
                {
                    REQUIRE_FALSE (plugin.preparedOffline);
                    REQUIRE_FALSE (plugin.isNonRealtime());
                    REQUIRE (plugin.liveBlocks > liveBefore);
                    REQUIRE (plugin.offlineBlocks == offlineAfterBounce);
                }
            }
        }
    }
}
