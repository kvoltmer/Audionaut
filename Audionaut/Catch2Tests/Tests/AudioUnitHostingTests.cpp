//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <JuceHeader.h>

// A real plugin behind the hosted effect: Apple's AUDelay, which every Mac
// has (and so every macOS CI runner), through the host's own Audio Unit
// format - the one format the sandboxed app hosts.
#if JUCE_MAC && JUCE_PLUGINHOST_AU

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_message.hpp>

#include <cmath>

#include "Cli/CliContext.h"
#include "Cli/Commands/Commands.h"
#include "Cli/HeadlessEngineSession.h"
#include "Engine/Effects/Hosting/HostedEffect.h"
#include "Engine/Effects/Hosting/PluginHost.h"
#include "Engine/Effects/Hosting/PluginTypeId.h"

using namespace audium;
using Catch::Approx;

namespace
{
    constexpr double kSampleRate = 44100.0;
    constexpr int kBlockSize = 512;
    const juce::String auDelayIdentifier = "AudioUnit:Effects/aufx,dely,appl";

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

    juce::AudioBuffer<float> makeSine (int numChannels, int numSamples, double hz)
    {
        juce::AudioBuffer<float> buffer (numChannels, numSamples);
        for (auto c = 0; c < numChannels; ++c)
            for (auto i = 0; i < numSamples; ++i)
                buffer.setSample (c, i, 0.5f * static_cast<float> (std::sin (2.0 * juce::MathConstants<double>::pi * hz * i / kSampleRate)));
        return buffer;
    }

    /// AUDelay's description from the format, as a scan would list it.
    std::unique_ptr<juce::PluginDescription> describeAuDelay (juce::AudioPluginFormatManager& formats)
    {
        for (auto* format : formats.getFormats())
        {
            if (format->getName() != "AudioUnit")
                continue;
            juce::OwnedArray<juce::PluginDescription> found;
            format->findAllTypesForFile (found, auDelayIdentifier);
            if (! found.isEmpty())
                return std::make_unique<juce::PluginDescription> (*found[0]);
        }
        return nullptr;
    }
}

SCENARIO ("Apple's AUDelay runs as a hosted effect", "[engine][effects][hosting][au]")
{
    MessageThread messageThread;

    GIVEN ("a plugin host on this Mac")
    {
        const auto temp = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getChildFile ("audionaut-au-host-" + juce::Uuid().toString());
        PluginHost host (temp);
        REQUIRE (host.getFormats().getNumFormats() >= 1);

        auto description = describeAuDelay (host.getFormats());
        if (description == nullptr)
        {
            WARN ("AUDelay is not available on this machine; skipping the Audio Unit scenario");
            temp.deleteRecursively();
            return;
        }

        THEN ("the description is what a project would store")
        {
            REQUIRE (description->pluginFormatName == "AudioUnit");
            REQUIRE (description->manufacturerName == "Apple");
            REQUIRE (description->name.containsIgnoreCase ("Delay"));
            REQUIRE (PluginTypeId::make (*description).rfind ("au:", 0) == 0);
            REQUIRE (host.canCreate (*description));
        }

        WHEN ("the host creates it as an effect and runs a sine through")
        {
            host.getList().addType (*description);
            auto effect = host.createEffect (*description, PluginHost::Mode::full);
            auto* hosted = dynamic_cast<HostedEffect*> (effect.get());
            REQUIRE (hosted != nullptr);
            REQUIRE (hosted->isLoaded());
            INFO (hosted->getError().toStdString());

            hosted->prepare (kSampleRate, kBlockSize, 2);
            REQUIRE (hosted->getStatus() == HostedEffect::Status::loaded);

            auto buffer = makeSine (2, kBlockSize * 8, 440.0);
            const auto dry = buffer;
            juce::dsp::AudioBlock<float> whole (buffer);
            for (size_t start = 0; start < whole.getNumSamples(); start += kBlockSize)
            {
                auto block = whole.getSubBlock (start, kBlockSize);
                hosted->process (block);
            }

            THEN ("the output is a signal, and not the input")
            {
                REQUIRE (buffer.getMagnitude (0, 0, buffer.getNumSamples()) > 0.01f);
                auto differs = false;
                for (auto i = kBlockSize; i < buffer.getNumSamples() && ! differs; ++i)
                    differs = std::abs (buffer.getSample (0, i) - dry.getSample (0, i)) > 1.0e-4f;
                REQUIRE (differs);
            }

            THEN ("its parameters have ids and read back as text, and its state round-trips")
            {
                REQUIRE (hosted->getNumParameters() >= 4);
                for (const auto& info : hosted->getParameters())
                    REQUIRE (std::string (info.id).length() > 0);
                REQUIRE (hosted->getParameterText (0, hosted->getParameter (0)).isNotEmpty());

                const auto state = hosted->getState();
                REQUIRE (state["blob"].is_string());
                REQUIRE_FALSE (state["blob"].get<std::string>().empty());
                hosted->setParameter (0, 0.9f);
                hosted->setState (state);
                REQUIRE (hosted->getState() == state);
            }

            THEN ("the planner would get a latency figure, zero or not")
            {
                REQUIRE (hosted->getLatencySamples() >= 0);
            }
        }

        WHEN ("the scan worker verb is run on it in this process")
        {
            cli::CliContext context;
            context.json = true;
            nlohmann::json envelope;
            context.envelopeSink = [&envelope] (const nlohmann::json& produced) { envelope = produced; };

            juce::ArgumentList args ("audionaut-cli", "scan-plugin --format AudioUnit " + auDelayIdentifier);
            cli::HeadlessEngineSession::setUseExternalMessageManager (true);   // this scenario owns the message thread
            const auto exitCode = cli::runScanPlugin (args, context);
            cli::HeadlessEngineSession::setUseExternalMessageManager (false);

            THEN ("it answers with the one description")
            {
                REQUIRE (exitCode == cli::exitOk);
                REQUIRE (envelope["ok"] == true);
                REQUIRE (envelope["result"]["plugins"].size() == 1);
                juce::PluginDescription parsed;
                auto xml = juce::parseXML (juce::String (envelope["result"]["plugins"][0].get<std::string>()));
                REQUIRE (xml != nullptr);
                REQUIRE (parsed.loadFromXml (*xml));
                REQUIRE (parsed.fileOrIdentifier == auDelayIdentifier);
            }
        }

        temp.deleteRecursively();
    }
}

SCENARIO ("the host runs an Audio Unit out of process when the format says so", "[engine][effects][hosting][au]")
{
    MessageThread messageThread;

    GIVEN ("a plugin host on this Mac and AUDelay")
    {
        const auto temp = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getChildFile ("audionaut-au-oop-" + juce::Uuid().toString());
        PluginHost host (temp);
        juce::PluginDescription description;
        description.pluginFormatName = "AudioUnit";
        description.fileOrIdentifier = auDelayIdentifier;
        description.name = "AUDelay";
        description.manufacturerName = "Apple";
        description.uniqueId = 0x64656c79;

        THEN ("the plugin is available, in-process: this test process is not sandboxed, and AUDelay is sandbox-safe anyway")
        {
            REQUIRE (host.availabilityOf (description) == PluginHost::Availability::available);
            REQUIRE (PluginHost::describe (PluginHost::Availability::available).isEmpty());
            REQUIRE (PluginHost::describe (PluginHost::Availability::formatMissing).isNotEmpty());
            REQUIRE_FALSE (host.loadsOutOfProcess (description));
            REQUIRE (host.getNumPendingLoads() == 0);
        }

        WHEN ("the format is told to run every plugin out of process, as the sandbox makes it do for an unsafe one")
        {
            auto* format = dynamic_cast<juce::AudioUnitPluginFormat*> (host.getFormats().getFormat (0));
            REQUIRE (format != nullptr);
            format->setProcessIsolation (juce::AudioUnitPluginFormat::ProcessIsolation::outOfProcess);
            REQUIRE (host.loadsOutOfProcess (description));

            THEN ("the effect starts as a loading placeholder and runs the plugin once it has arrived")
            {
                auto effect = host.createEffect (description, PluginHost::Mode::full);
                auto* hosted = dynamic_cast<HostedEffect*> (effect.get());
                REQUIRE (hosted != nullptr);
                REQUIRE (hosted->getStatus() == HostedEffect::Status::loading);
                REQUIRE_FALSE (hosted->isLoaded());
                REQUIRE (host.getNumPendingLoads() == 1);

                REQUIRE (host.waitForPendingLoads (20000));
                INFO (hosted->getError().toStdString());
                REQUIRE (hosted->getStatus() == HostedEffect::Status::loaded);
                REQUIRE (hosted->needsPrepare());
                REQUIRE (hosted->getNumParameters() >= 4);

                hosted->prepare (kSampleRate, kBlockSize, 2);
                REQUIRE_FALSE (hosted->needsPrepare());
                auto buffer = makeSine (2, kBlockSize * 8, 440.0);
                const auto dry = buffer;
                juce::dsp::AudioBlock<float> whole (buffer);
                for (size_t start = 0; start < whole.getNumSamples(); start += kBlockSize)
                {
                    auto block = whole.getSubBlock (start, kBlockSize);
                    hosted->process (block);
                }
                auto changed = false;
                for (auto i = 0; i < buffer.getNumSamples(); ++i)
                    changed = changed || std::abs (buffer.getSample (0, i) - dry.getSample (0, i)) > 1.0e-4f;
                REQUIRE (changed);
                hosted->release();
            }

            THEN ("an effect gone before its plugin arrives takes the plugin down with the callback")
            {
                auto effect = host.createEffect (description, PluginHost::Mode::full);
                effect.reset();
                REQUIRE (host.waitForPendingLoads (20000));
                REQUIRE (host.getNumPendingLoads() == 0);
            }
        }
        temp.deleteRecursively();
    }
}

SCENARIO ("an Audio Unit is loaded out of process on request", "[engine][effects][hosting][au]")
{
    MessageThread messageThread;

    GIVEN ("the Audio Unit format and AUDelay's identifier")
    {
        juce::AudioUnitPluginFormat format;
        juce::AudioPluginFormat& anyFormat = format;   // the creation rule is public on the base class only
        juce::PluginDescription auDelay;
        auDelay.fileOrIdentifier = auDelayIdentifier;
        auDelay.pluginFormatName = format.getName();
        if (! format.fileMightContainThisPluginType (auDelayIdentifier))
        {
            WARN ("AUDelay is not available on this machine; skipping the out-of-process scenario");
            return;
        }

        THEN ("by default a sandbox-safe AUv2 is created in this process, synchronously")
        {
            REQUIRE (format.getProcessIsolation() == juce::AudioUnitPluginFormat::ProcessIsolation::inProcess);
            REQUIRE_FALSE (anyFormat.requiresUnblockedMessageThreadDuringCreation (auDelay));
            REQUIRE (format.searchPathsForPlugins ({}, false, false).contains (auDelayIdentifier));
        }

        THEN ("this test process is not sandboxed, so the sandbox rule changes nothing for it")
        {
            format.setProcessIsolation (juce::AudioUnitPluginFormat::ProcessIsolation::whenSandboxRequires);
            REQUIRE_FALSE (anyFormat.requiresUnblockedMessageThreadDuringCreation (auDelay));
        }

        WHEN ("every plugin is to run out of process")
        {
            format.setProcessIsolation (juce::AudioUnitPluginFormat::ProcessIsolation::outOfProcess);

            THEN ("the plugin needs asynchronous creation, like an AUv3")
            {
                REQUIRE (anyFormat.requiresUnblockedMessageThreadDuringCreation (auDelay));
                REQUIRE_FALSE (format.searchPathsForPlugins ({}, false, false).contains (auDelayIdentifier));
                REQUIRE (format.searchPathsForPlugins ({}, false, true).contains (auDelayIdentifier));
            }

            THEN ("it is created in the system's hosting service and renders from there")
            {
                std::unique_ptr<juce::AudioPluginInstance> instance;
                juce::String error;
                auto arrived = false;
                format.createPluginInstanceAsync (auDelay, kSampleRate, kBlockSize,
                                                  [&] (std::unique_ptr<juce::AudioPluginInstance> created, const juce::String& message)
                                                  {
                                                      instance = std::move (created);
                                                      error = message;
                                                      arrived = true;
                                                  });
                const auto deadline = juce::Time::getMillisecondCounter() + 20000;
                while (! arrived && juce::Time::getMillisecondCounter() < deadline)
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (20);

                INFO (error.toStdString());
                REQUIRE (arrived);
                REQUIRE (instance != nullptr);
                REQUIRE (instance->getName().containsIgnoreCase ("Delay"));
                REQUIRE (instance->getParameters().size() >= 4);

                instance->setPlayConfigDetails (2, 2, kSampleRate, kBlockSize);
                instance->prepareToPlay (kSampleRate, kBlockSize);
                auto buffer = makeSine (2, kBlockSize * 8, 440.0);
                const auto dry = buffer;
                juce::MidiBuffer midi;
                for (auto start = 0; start < buffer.getNumSamples(); start += kBlockSize)
                {
                    juce::AudioBuffer<float> block (buffer.getArrayOfWritePointers(), 2, start, kBlockSize);
                    instance->processBlock (block, midi);
                }
                instance->releaseResources();

                auto changed = false;
                auto silent = true;
                for (auto i = 0; i < buffer.getNumSamples(); ++i)
                {
                    changed = changed || std::abs (buffer.getSample (0, i) - dry.getSample (0, i)) > 1.0e-4f;
                    silent = silent && std::abs (buffer.getSample (0, i)) < 1.0e-6f;
                }
                REQUIRE (changed);
                REQUIRE_FALSE (silent);
            }
        }
    }
}

#endif
