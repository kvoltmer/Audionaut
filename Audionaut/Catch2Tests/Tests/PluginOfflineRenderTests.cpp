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
#include "Engine/Export/AudioExporter.h"
#include "Engine/Export/ExportAudioConfig.h"
#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/PlayList/PlayListContainer.h"
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
 * described as a plugin of `format`, and counts the blocks it ran with the
 * non-realtime flag set and without.
 */
struct Session
{
    explicit Session (const juce::String& format = "TestA")
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
        registry.add ({ kTypeId, "Offline Test" }, [this, &registry, format]
        {
            TestPluginInstance::Options options;
            options.format = format;
            auto instance = std::make_unique<TestPluginInstance> (options);
            plugin = instance.get();
            const auto description = TestPluginInstance::describe (options);
            auto effect = std::make_unique<HostedEffect> (PluginTypeId::typeInfo (description), description, std::move (instance));
            effect->setRenderTransport (registry.getRenderTransport());
            hosted = effect.get();
            return effect;
        });
        REQUIRE (container->getAudioTrack (0)->getTrackInserts().add (kTypeId) != nullptr);
        REQUIRE (plugin != nullptr);

        engine->getPlayListScheduler()->commitPlayListData();
    }

    MixerPlanner& planner() { return *engine->getAudioTrackContainer()->getMixerPlanner(); }

    std::shared_ptr<AudiumEngine> engine;
    TestPluginInstance* plugin = nullptr;   // owned by the insert slot's effect
    HostedEffect* hosted = nullptr;         // the effect itself, owned by the slot
};

/// A device type for the engine's device manager: one device, which never calls back.
class FakeDeviceType final : public juce::AudioIODeviceType
{
public:
    FakeDeviceType (double sampleRate_, int blockSize_) :
        juce::AudioIODeviceType ("fake"), sampleRate (sampleRate_), blockSize (blockSize_) {}

    void scanForDevices() override {}
    juce::StringArray getDeviceNames (bool) const override { return { "fake" }; }
    int getDefaultDeviceIndex (bool) const override { return 0; }
    int getIndexOfDevice (juce::AudioIODevice* device, bool) const override { return device != nullptr ? 0 : -1; }
    bool hasSeparateInputsAndOutputs() const override { return false; }
    juce::AudioIODevice* createDevice (const juce::String&, const juce::String&) override
    {
        return new FakeAudioIODevice (sampleRate, blockSize, 2, 2);
    }

private:
    const double sampleRate;
    const int blockSize;
};

/// A fake device of that format as the engine's current device, with the engine's callback on it as
/// the app puts it there (AudiumEngine::initialise): the callback hears of the device and prepares for it.
struct OpenFakeDevice
{
    OpenFakeDevice (AudiumEngine& engine_, double sampleRate, int blockSize) : engine (engine_)
    {
        auto manager = engine.getAudioDeviceManager();
        manager->addAudioDeviceType (std::make_unique<FakeDeviceType> (sampleRate, blockSize));
        manager->setCurrentAudioDeviceType ("fake", true);
        const auto error = manager->initialise (2, 2, nullptr, true);
        CAPTURE (error.toStdString());
        REQUIRE (manager->getCurrentAudioDevice() != nullptr);
        REQUIRE (manager->getCurrentAudioDevice()->getCurrentBufferSizeSamples() == blockSize);
        manager->addAudioCallback (engine.getLinkAudioDevice().get());
    }

    ~OpenFakeDevice()
    {
        auto manager = engine.getAudioDeviceManager();
        manager->removeAudioCallback (engine.getLinkAudioDevice().get());
        manager->closeAudioDevice();
    }

    AudiumEngine& engine;
};

} // namespace

SCENARIO ("a plugin is told when it runs in a bounce, and when in playback again", "[engine][effects][hosting][offline]")
{
    MessageThread messageThread;

    GIVEN ("a project with a plugin on its track, behind the device callback")
    {
        Session session;
        auto& plugin = *session.plugin;
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

            THEN ("the plugin ran the whole bounce in offline mode")
            {
                REQUIRE (plugin.offlineBlocks >= 21);   // 11025 samples in blocks of 512
                REQUIRE (plugin.liveBlocks == liveBefore);
            }

            THEN ("a plugin whose format has the mode at once was not prepared again for it")
            {
                REQUIRE (plugin.prepareCount == preparesBefore);
            }

            THEN ("the plugin is back in live mode once the bounce is over, with no device to start for it")
            {
                REQUIRE_FALSE (session.planner().isOfflineRendering());
                REQUIRE_FALSE (session.hosted->isOfflineRendering());
                REQUIRE_FALSE (plugin.isNonRealtime());
            }

            AND_WHEN ("the transport plays again")
            {
                const auto offlineAfterBounce = plugin.offlineBlocks;
                driver.play (0.0);
                driver.render (0.1);
                driver.stop();

                THEN ("the plugin runs live")
                {
                    REQUIRE (plugin.liveBlocks > liveBefore);
                    REQUIRE (plugin.offlineBlocks == offlineAfterBounce);
                }
            }
        }

        WHEN ("one clip is bounced, which runs no insert")
        {
            driver.idle (2);
            const auto preparesBefore = plugin.prepareCount;
            const auto processedBefore = plugin.processCount;

            auto config = std::make_shared<ExportAudioConfig>();
            config->fileName = juce::File::createTempFile ("-offline-clip.wav");
            config->sampleRate = kSampleRate;
            config->blockSize = kBlockSize;
            config->playListItem = session.engine->getAudioTrackContainer()->getAudioTrack (0)
                                       ->getPlayListContainer()->getPlayListItems().front();
            REQUIRE (AudioExporter (*session.engine, config).bounce());
            config->fileName.deleteFile();

            THEN ("the plugin is left alone: not told of a bounce, not prepared, not run")
            {
                REQUIRE_FALSE (plugin.isNonRealtime());
                REQUIRE (plugin.offlineBlocks == 0);
                REQUIRE (plugin.prepareCount == preparesBefore);
                REQUIRE (plugin.processCount == processedBefore);
            }
        }
    }

    GIVEN ("a project with a plugin of a format that reads the mode when it is prepared")
    {
        Session session ("VST3");
        auto& plugin = *session.plugin;
        FakeAudioIODevice device (kSampleRate, kBlockSize, 2, 2);
        LiveBlockDriver driver (*session.engine, device);
        driver.idle (2);
        REQUIRE_FALSE (plugin.preparedOffline);

        WHEN ("the project is bounced with the device's block size and rate")
        {
            const auto preparesBefore = plugin.prepareCount;
            bounceProject (*session.engine, 0.0, 0.25, kBlockSize, kSampleRate, 2);

            THEN ("the plugin was prepared for the bounce in offline mode, and for live rendering after it")
            {
                REQUIRE (plugin.offlineBlocks >= 21);
                REQUIRE (plugin.prepareCount == preparesBefore + 2);
                REQUIRE_FALSE (plugin.preparedOffline);
                REQUIRE_FALSE (plugin.isNonRealtime());
            }
        }
    }
}

SCENARIO ("after a bounce the plugins are back in live mode at the device's format", "[engine][effects][hosting][offline]")
{
    MessageThread messageThread;

    GIVEN ("a project with a plugin on its track and a device open at 256 samples")
    {
        Session session ("VST3");
        auto& plugin = *session.plugin;
        constexpr int deviceBlockSize = 256;
        OpenFakeDevice openDevice (*session.engine, kSampleRate, deviceBlockSize);
        REQUIRE (session.planner().getMaxBlockSize() == deviceBlockSize);
        REQUIRE_FALSE (plugin.preparedOffline);

        WHEN ("the project is bounced in blocks of 512")
        {
            bounceProject (*session.engine, 0.0, 0.25, kBlockSize, kSampleRate, 2);

            THEN ("the bounce ran offline, and the plugin is prepared for the device again, live")
            {
                REQUIRE (plugin.offlineBlocks >= 21);
                REQUIRE (plugin.liveBlocks == 0);
                REQUIRE (session.planner().getMaxBlockSize() == deviceBlockSize);
                REQUIRE_FALSE (session.planner().isOfflineRendering());
                REQUIRE_FALSE (session.hosted->isOfflineRendering());
                REQUIRE_FALSE (plugin.isNonRealtime());
                REQUIRE_FALSE (plugin.preparedOffline);
            }
        }
    }
}
