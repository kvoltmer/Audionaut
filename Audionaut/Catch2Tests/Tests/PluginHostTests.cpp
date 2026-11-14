//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Engine/Effects/EffectRegistry.h"
#include "Engine/Effects/Hosting/ChildProcessScanner.h"
#include "Engine/Effects/Hosting/HostedEffect.h"
#include "Engine/Effects/Hosting/PluginHost.h"
#include "Engine/Effects/Hosting/PluginTypeId.h"
#include "Engine/Effects/InsertChain.h"
#include "Engine/Effects/MixPlanContainer.h"
#include "Engine/Effects/MixerPlanner.h"
#include "TestPluginInstance.h"

using namespace audium;

namespace
{
    /// A folder of its own for the host's list, gone with the scenario.
    struct TempFolder
    {
        TempFolder() :
            folder (juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("audionaut-plugin-host-" + juce::Uuid().toString()))
        {
            folder.createDirectory();
        }
        ~TempFolder() { folder.deleteRecursively(); }
        juce::File folder;
    };

    /// The list changes are broadcast through the message thread: one for the scenario.
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
        void pump()
        {
            for (auto i = 0; i < 5; ++i)
                juce::MessageManager::getInstance()->runDispatchLoopUntil (5);
        }
        std::unique_ptr<juce::MessageManagerLock> lock;
    };

    TestPluginInstance::Options gainIn (const juce::String& format, int uid)
    {
        TestPluginInstance::Options options;
        options.format = format;
        options.uid = uid;
        return options;
    }

    /// A host whose formats are two test formats, "TestA" holding a gain plugin and "TestB" the same plugin again.
    struct TestHost
    {
        explicit TestHost (const juce::File& folder, bool listThem = true) :
            host (folder, false)   // the test formats only: a scan must not go through this machine's plugins
        {
            host.getFormats().addFormat (std::make_unique<TestPluginFormat> ("TestA", std::vector { gainIn ("TestA", 0x1111) }));
            host.getFormats().addFormat (std::make_unique<TestPluginFormat> ("TestB", std::vector { gainIn ("TestB", 0x2222) }));
            if (listThem)
            {
                host.getList().addType (TestPluginInstance::describe (gainIn ("TestA", 0x1111)));
                host.getList().addType (TestPluginInstance::describe (gainIn ("TestB", 0x2222)));
            }
        }
        PluginHost host;
    };
}

SCENARIO ("the plugin host keeps its list between runs", "[engine][effects][hosting]")
{
    MessageThread messageThread;
    TempFolder temp;

    GIVEN ("a host with two listed plugins and a blacklisted file")
    {
        TestHost first (temp.folder);
        first.host.getList().addToBlacklist ("/test/crashes.testa");
        REQUIRE (first.host.saveList());
        REQUIRE (first.host.getListFile().existsAsFile());
        REQUIRE (first.host.getListFile().getParentDirectory().getFileName() == "Plugins");

        WHEN ("another host in the same folder loads the list")
        {
            TestHost second (temp.folder, false);
            REQUIRE (second.host.getList().getNumTypes() == 0);
            REQUIRE (second.host.loadList());

            THEN ("it has the plugins and the blacklist")
            {
                REQUIRE (second.host.getList().getNumTypes() == 2);
                REQUIRE (second.host.getList().getBlacklistedFiles().contains ("/test/crashes.testa"));
                REQUIRE (second.host.findByTypeId ("testa:00001111").has_value());
                REQUIRE (second.host.findByTypeId ("testb:00002222")->pluginFormatName == "TestB");
                REQUIRE_FALSE (second.host.findByTypeId ("testa:00009999").has_value());
            }
        }

        WHEN ("a host starts in an empty folder")
        {
            TempFolder other;
            PluginHost empty (other.folder, false);

            THEN ("there is no list to load, and nothing is listed")
            {
                REQUIRE_FALSE (empty.loadList());
                REQUIRE (empty.getList().getNumTypes() == 0);
            }
        }
    }
}

SCENARIO ("the plugin host registers its plugins as effect types", "[engine][effects][hosting]")
{
    MessageThread messageThread;
    TempFolder temp;
    TestHost test (temp.folder);
    auto& host = test.host;

    GIVEN ("a registry with the built-ins")
    {
        auto registry = std::make_shared<EffectRegistry>();
        registry->addBuiltInEffects();
        const auto numBuiltIns = static_cast<int> (registry->getTypes().size());

        WHEN ("the host registers into it")
        {
            host.registerInto (*registry, PluginHost::Mode::full);

            THEN ("every plugin is a type after the built-ins, listed under its manufacturer, the second format marked")
            {
                const auto& types = registry->getTypes();
                REQUIRE (static_cast<int> (types.size()) == numBuiltIns + 2);
                REQUIRE (types[static_cast<size_t> (numBuiltIns)].id == "testa:00001111");
                REQUIRE (types[static_cast<size_t> (numBuiltIns)].name == "Test Gain");
                REQUIRE (types[static_cast<size_t> (numBuiltIns)].category == "Audionaut Tests");
                REQUIRE (types[static_cast<size_t> (numBuiltIns) + 1].id == "testb:00002222");
                REQUIRE (types[static_cast<size_t> (numBuiltIns) + 1].name == "Test Gain (TestB)");
            }

            THEN ("creating a type loads the plugin as a hosted effect")
            {
                auto effect = registry->create ("testa:00001111");
                REQUIRE (effect != nullptr);
                auto* hosted = dynamic_cast<HostedEffect*> (effect.get());
                REQUIRE (hosted != nullptr);
                REQUIRE (hosted->isLoaded());
                REQUIRE (hosted->getNumParameters() == 2);
                REQUIRE (hosted->getType().id == "testa:00001111");
            }

            THEN ("a plugin that leaves the list leaves the registry on the next registration, the built-ins stay")
            {
                host.getList().removeType (TestPluginInstance::describe (gainIn ("TestB", 0x2222)));
                host.registerInto (*registry, PluginHost::Mode::full);
                REQUIRE (static_cast<int> (registry->getTypes().size()) == numBuiltIns + 1);
                REQUIRE (registry->hasType ("testa:00001111"));
                REQUIRE_FALSE (registry->hasType ("testb:00002222"));
                REQUIRE (registry->hasType ("builtin:trim"));
            }
        }

        WHEN ("the host registers in catalogue-only mode")
        {
            host.registerInto (*registry, PluginHost::Mode::catalogueOnly);
            const auto instancesBefore = TestPluginInstance::instances();

            THEN ("the types are there, but creating one gives a placeholder without loading the plugin")
            {
                REQUIRE (registry->hasType ("testa:00001111"));
                auto effect = registry->create ("testa:00001111");
                auto* hosted = dynamic_cast<HostedEffect*> (effect.get());
                REQUIRE (hosted != nullptr);
                REQUIRE_FALSE (hosted->isLoaded());
                REQUIRE (TestPluginInstance::instances() == instancesBefore);
                REQUIRE (hosted->getPluginInfo()["name"] == "Test Gain");
            }
        }

        WHEN ("the host is attached and the list changes")
        {
            host.attach (registry, PluginHost::Mode::full);
            REQUIRE (registry->hasType ("testb:00002222"));
            host.getList().removeType (TestPluginInstance::describe (gainIn ("TestB", 0x2222)));
            messageThread.pump();

            THEN ("the registry follows, and the list is saved")
            {
                REQUIRE_FALSE (registry->hasType ("testb:00002222"));
                REQUIRE (registry->hasType ("testa:00001111"));
                REQUIRE (host.getListFile().existsAsFile());
            }
        }

        WHEN ("a listed plugin's format creates it asynchronously only (a plugin in a process of its own)")
        {
            auto asyncFormat = std::make_unique<TestPluginFormat> ("TestC", std::vector { gainIn ("TestC", 0x3333) });
            asyncFormat->asyncOnly = true;
            host.getFormats().addFormat (std::move (asyncFormat));
            const auto description = TestPluginInstance::describe (gainIn ("TestC", 0x3333));
            host.getList().addType (description);
            host.registerInto (*registry, PluginHost::Mode::full);

            THEN ("it is a type like any other, marked as running out of process")
            {
                REQUIRE (host.getList().getNumTypes() == 3);
                REQUIRE (registry->hasType ("testc:00003333"));
                REQUIRE (host.canCreate (description));
                REQUIRE (host.loadsOutOfProcess (description));
                REQUIRE_FALSE (host.loadsOutOfProcess (TestPluginInstance::describe (gainIn ("TestA", 0x1111))));
            }

            THEN ("creating it gives a loading placeholder that adopts the plugin from the message queue")
            {
                auto effect = registry->create ("testc:00003333");
                auto* hosted = dynamic_cast<HostedEffect*> (effect.get());
                REQUIRE (hosted != nullptr);
                REQUIRE (hosted->getStatus() == HostedEffect::Status::loading);
                REQUIRE (hosted->getNumParameters() == 0);
                REQUIRE (host.getNumPendingLoads() == 1);

                REQUIRE (host.waitForPendingLoads (5000));
                REQUIRE (host.getNumPendingLoads() == 0);
                REQUIRE (hosted->getStatus() == HostedEffect::Status::loaded);
                REQUIRE (hosted->isLoaded());
                REQUIRE (hosted->getNumParameters() == 2);
            }

            THEN ("a placeholder destroyed before the plugin arrives leaves nothing behind")
            {
                auto effect = registry->create ("testc:00003333");
                REQUIRE (host.getNumPendingLoads() == 1);
                effect.reset();
                REQUIRE (host.waitForPendingLoads (5000));
                REQUIRE (host.getNumPendingLoads() == 0);
            }

            THEN ("a catalogue-only engine gets a plain placeholder, nothing pending")
            {
                auto effect = host.createEffect (description, PluginHost::Mode::catalogueOnly);
                auto* hosted = dynamic_cast<HostedEffect*> (effect.get());
                REQUIRE (hosted != nullptr);
                REQUIRE (hosted->getStatus() == HostedEffect::Status::notLoaded);
                REQUIRE (host.getNumPendingLoads() == 0);
            }
        }
    }
}

SCENARIO ("a project's plugin insert opens with the same plugin in another format", "[engine][effects][hosting][inserts]")
{
    MessageThread messageThread;
    TempFolder temp;

    GIVEN ("a host that has the plugin as TestB only, registered in a chain's planner")
    {
        PluginHost host (temp.folder, false);
        host.getFormats().addFormat (std::make_unique<TestPluginFormat> ("TestA"));   // the format is here, this plugin is not
        host.getFormats().addFormat (std::make_unique<TestPluginFormat> ("TestB", std::vector { gainIn ("TestB", 0x2222) }));
        host.getList().addType (TestPluginInstance::describe (gainIn ("TestB", 0x2222)));

        auto registry = std::make_shared<EffectRegistry>();
        registry->addBuiltInEffects();
        host.registerInto (*registry, PluginHost::Mode::full);
        MixerPlanner planner (registry, std::make_shared<MixPlanContainer>());
        InsertChain chain (&planner);

        WHEN ("a chain saved with the TestA plugin is read")
        {
            json input;
            auto element = json { { "type", "testa:00001111" },
                                  { "enabled", true },
                                  { "plugin", PluginTypeId::pluginInfo (TestPluginInstance::describe (gainIn ("TestA", 0x1111))) },
                                  { "state", { { "blob", juce::MemoryBlock ("1.500000|0|0", 12).toBase64Encoding().toStdString() } } } };
            input["inserts"] = json::array ({ element });
            chain.readFromJson (input);

            THEN ("the slot holds the TestB plugin with the saved state, and the chain writes the new id")
            {
                REQUIRE (chain.size() == 1);
                REQUIRE (chain.getNumUnresolved() == 0);
                auto* hosted = dynamic_cast<HostedEffect*> (&chain.getSlot (0)->getEffect());
                REQUIRE (hosted != nullptr);
                REQUIRE (hosted->getType().id == "testb:00002222");
                REQUIRE (hosted->isLoaded());
                REQUIRE (hosted->getParameter (0) == Catch::Approx (0.75f));   // gain 1.5 of 0..2

                json output;
                chain.writeToJson (output);
                REQUIRE (output["inserts"].size() == 1);
                REQUIRE (output["inserts"][0]["type"] == "testb:00002222");
                REQUIRE (output["inserts"][0]["plugin"]["format"] == "TestB");
                REQUIRE (output["inserts"][0]["plugin"]["name"] == "Test Gain");
                REQUIRE (output["inserts"][0]["state"]["blob"].is_string());
            }
        }

        WHEN ("a chain saved with a plugin nobody here has is read")
        {
            auto options = gainIn ("TestA", 0x4444);
            options.name = "Nope";
            json input;
            input["inserts"] = json::array ({ { { "type", "testa:00004444" },
                                                { "enabled", false },
                                                { "plugin", PluginTypeId::pluginInfo (TestPluginInstance::describe (options)) },
                                                { "state", { { "blob", "QUJD" } } } } });
            chain.readFromJson (input);

            THEN ("the element is kept as it was, and can be dropped")
            {
                REQUIRE (chain.isEmpty());
                REQUIRE (chain.getNumUnresolved() == 1);
                REQUIRE (chain.getUnresolved()[0].element["plugin"]["name"] == "Nope");
                REQUIRE (chain.getUnresolved()[0].index == 0);

                json output;
                chain.writeToJson (output);
                REQUIRE (output["inserts"][0]["type"] == "testa:00004444");
                REQUIRE (output["inserts"][0]["state"]["blob"] == "QUJD");

                REQUIRE_FALSE (chain.removeUnresolved (1));
                REQUIRE (chain.removeUnresolved (0));
                REQUIRE (chain.getNumUnresolved() == 0);
            }
        }

        WHEN ("an element without a plugin block names an unknown type")
        {
            json input;
            input["inserts"] = json::array ({ { { "type", "builtin:future" }, { "enabled", true }, { "state", json::object() } } });
            chain.readFromJson (input);

            THEN ("there is nothing to substitute, it is kept aside as before")
            {
                REQUIRE (chain.isEmpty());
                REQUIRE (chain.getNumUnresolved() == 1);
            }
        }
    }
}

SCENARIO ("the plugin host scans with its formats", "[engine][effects][hosting]")
{
    MessageThread messageThread;
    TempFolder temp;

    GIVEN ("a host with two test formats and an empty list")
    {
        TestHost test (temp.folder, false);
        auto& host = test.host;
        REQUIRE (host.getList().getNumTypes() == 0);

        WHEN ("it scans")
        {
            std::vector<juce::String> files;
            const auto result = host.scanAll ([&] (float, const juce::String& file) { files.push_back (file); });

            THEN ("both plugins are found and the list is saved")
            {
                REQUIRE (result.found == 2);
                REQUIRE (result.added == 2);
                REQUIRE (result.failed == 0);
                REQUIRE (result.blacklisted == 0);
                REQUIRE (host.getList().getNumTypes() == 2);
                REQUIRE (host.getListFile().existsAsFile());
                REQUIRE_FALSE (files.empty());
            }
        }

        WHEN ("it scans one format")
        {
            const auto result = host.scanAll ({}, {}, "TestB");

            THEN ("only that format's plugins are found")
            {
                REQUIRE (result.found == 1);
                REQUIRE (host.findByTypeId ("testb:00002222").has_value());
            }
        }
    }
}

SCENARIO ("the child-process scanner reads the worker's envelope", "[engine][effects][hosting]")
{
    GIVEN ("the envelope the scan-plugin worker writes")
    {
        const auto description = TestPluginInstance::describe (gainIn ("VST3", 0x5555));
        const auto xml = description.createXml()->toString().toStdString();
        const auto envelope = json { { "ok", true }, { "result", { { "plugins", json::array ({ xml }) } } } };

        THEN ("the descriptions come out of it")
        {
            juce::OwnedArray<juce::PluginDescription> found;
            REQUIRE (ChildProcessScanner::parseScanEnvelope (juce::String (envelope.dump()), found));
            REQUIRE (found.size() == 1);
            REQUIRE (found[0]->name == "Test Gain");
            REQUIRE (found[0]->uniqueId == 0x5555);
            REQUIRE (found[0]->pluginFormatName == "VST3");
        }

        THEN ("a plugin's chatter before the envelope is skipped")
        {
            juce::OwnedArray<juce::PluginDescription> found;
            REQUIRE (ChildProcessScanner::parseScanEnvelope ("loading...\nlicence ok\n" + juce::String (envelope.dump()), found));
            REQUIRE (found.size() == 1);
        }

        THEN ("a worker that declined cleanly is no crash, but has nothing")
        {
            juce::OwnedArray<juce::PluginDescription> found;
            const auto declined = json { { "ok", false }, { "error", { { "code", "unknown_format" }, { "message", "nope" } } } };
            REQUIRE (ChildProcessScanner::parseScanEnvelope (juce::String (declined.dump()), found));
            REQUIRE (found.isEmpty());
        }

        THEN ("no envelope at all is a crash")
        {
            juce::OwnedArray<juce::PluginDescription> found;
            REQUIRE_FALSE (ChildProcessScanner::parseScanEnvelope ("", found));
            REQUIRE_FALSE (ChildProcessScanner::parseScanEnvelope ("Segmentation fault", found));
            REQUIRE_FALSE (ChildProcessScanner::parseScanEnvelope ("{\"ok\": true, \"result\": {}", found));
        }
    }
}
