//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

// before JUCE, whose names (Point, AudioBuffer) the system's headers would stumble over
#if defined (__APPLE__)
 #include <TargetConditionals.h>
 #if TARGET_OS_OSX
  #include <AudioToolbox/AudioToolbox.h>
 #endif
#endif

#include <JuceHeader.h>

// What a bounce tells a real Audio Unit, asked for the offline render
// property the host is to set - the plugin wrapper's non-realtime flag alone
// never reaches the unit. Few units have the property (a host's attempt to
// set it on the others is refused and changes nothing); of Apple's effects
// AURoundTripAAC does, so that is the one asked.
#if JUCE_MAC && JUCE_PLUGINHOST_AU

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_message.hpp>

#include "Engine/Effects/Hosting/HostedEffect.h"
#include "Engine/Effects/Hosting/PluginHost.h"

using namespace audium;

namespace
{
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
    };

    constexpr UInt32 noSuchProperty = 99;

    /// The unit's own answer: 1 while it is to render offline, 0 otherwise; noSuchProperty from a unit without it.
    UInt32 offlineRenderOf (const HostedEffect& hosted)
    {
        auto* client = hosted.getInstance()->getAudioUnitClient();
        REQUIRE (client != nullptr);
        UInt32 value = 0;
        UInt32 size = sizeof (value);
        if (AudioUnitGetProperty (client->getAudioUnitHandle(), kAudioUnitProperty_OfflineRender,
                                  kAudioUnitScope_Global, 0, &value, &size) != noErr)
            return noSuchProperty;
        return value;
    }
}

SCENARIO ("an Audio Unit itself hears of a bounce, and of its end", "[engine][effects][hosting][au][offline]")
{
    MessageThread messageThread;

    GIVEN ("Apple's AURoundTripAAC as a hosted effect, prepared for live rendering")
    {
        const auto temp = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getChildFile ("audionaut-au-offline-" + juce::Uuid().toString());
        PluginHost host (temp);

        std::unique_ptr<juce::PluginDescription> description;
        for (auto* format : host.getFormats().getFormats())
        {
            if (format->getName() != "AudioUnit")
                continue;
            juce::OwnedArray<juce::PluginDescription> found;
            format->findAllTypesForFile (found, "AudioUnit:Effects/aufx,raac,appl");
            if (! found.isEmpty())
                description = std::make_unique<juce::PluginDescription> (*found[0]);
        }
        if (description == nullptr)
        {
            WARN ("AURoundTripAAC is not available on this machine; skipping the Audio Unit scenario");
            temp.deleteRecursively();
            return;
        }

        host.getList().addType (*description);
        auto effect = host.createEffect (*description, PluginHost::Mode::full);
        auto* hosted = dynamic_cast<HostedEffect*> (effect.get());
        REQUIRE (hosted != nullptr);
        REQUIRE (hosted->isLoaded());
        hosted->prepare (44100.0, 512, 2);
        REQUIRE (hosted->getStatus() == HostedEffect::Status::loaded);
        if (offlineRenderOf (*hosted) == noSuchProperty)
        {
            WARN ("AURoundTripAAC has no offline render property on this system; skipping the Audio Unit scenario");
            effect.reset();
            temp.deleteRecursively();
            return;
        }
        REQUIRE (offlineRenderOf (*hosted) == 0);

        WHEN ("the effect hears that a bounce comes")
        {
            hosted->setOfflineRendering (true);

            THEN ("the unit is set to render offline, and stays so through the prepare that follows")
            {
                REQUIRE (offlineRenderOf (*hosted) == 1);
                REQUIRE (hosted->needsPrepare());
                hosted->prepare (44100.0, 512, 2);
                REQUIRE (hosted->getStatus() == HostedEffect::Status::loaded);
                REQUIRE (offlineRenderOf (*hosted) == 1);
            }

            AND_WHEN ("the bounce is over")
            {
                hosted->setOfflineRendering (false);

                THEN ("the unit renders live again")
                {
                    REQUIRE (offlineRenderOf (*hosted) == 0);
                }
            }
        }

        effect.reset();
        temp.deleteRecursively();
    }
}

#endif
