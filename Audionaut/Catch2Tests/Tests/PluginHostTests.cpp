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

        WHEN ("a second host loads the list, and the first then finds and saves another plugin")
        {
            TestHost second (temp.folder, false);
            REQUIRE (second.host.loadList());

            first.host.getList().addType (TestPluginInstance::describe (gainIn ("TestA", 0x3333)));
            REQUIRE (first.host.saveList());

            THEN ("the second, which changed nothing, leaves the newer file alone when asked to save what changed")
            {
                REQUIRE (second.host.saveListIfChanged());
                TestHost third (temp.folder, false);
                REQUIRE (third.host.loadList());
                REQUIRE (third.host.getList().getNumTypes() == 3);
            }

            AND_WHEN ("the second changes its own list")
            {
                second.host.getList().addToBlacklist ("/test/also-crashes.testa");

                THEN ("that is written")
                {
                    REQUIRE (second.host.saveListIfChanged());
                    TestHost third (temp.folder, false);
                    REQUIRE (third.host.loadList());
                    REQUIRE (third.host.getList().getBlacklistedFiles().contains ("/test/also-crashes.testa"));
                }
            }
        }

        WHEN ("a placeholder made without loading the plugin is given its plugin after all")
        {
            const auto description = TestPluginInstance::describe (gainIn ("TestA", 0x1111));
            const auto instancesBefore = TestPluginInstance::instances();
            auto effect = first.host.createEffect (description, PluginHost::Mode::catalogueOnly);
            auto* hosted = dynamic_cast<HostedEffect*> (effect.get());
            REQUIRE (hosted != nullptr);
            REQUIRE_FALSE (hosted->isLoaded());
            REQUIRE (hosted->getNumParameters() == 0);
            REQUIRE (TestPluginInstance::instances() == instancesBefore);
            hosted->setState (json { { "blob", juce::MemoryBlock ("1.500000|0|0", 12).toBase64Encoding().toStdString() } });

            THEN ("the plugin is there, with its parameters and the state the placeholder kept")
            {
                REQUIRE (first.host.load (*hosted));
                REQUIRE (hosted->isLoaded());
                REQUIRE (hosted->getStatus() == HostedEffect::Status::loaded);
                REQUIRE (TestPluginInstance::instances() == instancesBefore + 1);
                REQUIRE (hosted->getParameterIndex ("gain") == 0);
                REQUIRE (hosted->getParameter (0) == Catch::Approx (0.75f));   // gain 1.5 of 0..2
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

        WHEN ("a chain saved with two missing plugins in front of one that is here is read")
        {
            const auto missing = [] (const char* type, const char* blob)
            {
                return json { { "type", type }, { "enabled", true }, { "state", { { "blob", blob } } } };
            };
            json input;
            input["inserts"] = json::array ({ missing ("testa:0000aaaa", "QQ=="), missing ("testa:0000bbbb", "Qg=="),
                                              { { "type", "testb:00002222" }, { "enabled", true }, { "state", { { "blob", "" } } } } });
            chain.readFromJson (input);
            REQUIRE (chain.size() == 1);
            REQUIRE (chain.getNumUnresolved() == 2);

            AND_WHEN ("the first missing one is removed")
            {
                REQUIRE (chain.removeUnresolved (0));

                THEN ("the other missing one still stands in front of the plugin when the chain is written")
                {
                    json output;
                    chain.writeToJson (output);
                    REQUIRE (output["inserts"].size() == 2);
                    REQUIRE (output["inserts"][0]["type"] == "testa:0000bbbb");
                    REQUIRE (output["inserts"][1]["type"] == "testb:00002222");
                }
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

        THEN ("chatter with braces of its own does not hide the envelope")
        {
            juce::OwnedArray<juce::PluginDescription> found;
            REQUIRE (ChildProcessScanner::parseScanEnvelope ("init {mode: fast}\n{\"licence\": \"ok\"}\n" + juce::String (envelope.dump (2)), found));
            REQUIRE (found.size() == 1);
            REQUIRE (found[0]->uniqueId == 0x5555);
        }

        THEN ("a worker that declined cleanly is no crash, but has nothing")
        {
            juce::OwnedArray<juce::PluginDescription> found;
            const auto declined = json { { "ok", false }, { "error", { { "code", "unknown_format" }, { "message", "nope" } } } };
            REQUIRE (ChildProcessScanner::parseScanEnvelope (juce::String (declined.dump()), found));
            REQUIRE (found.isEmpty());

            const auto stuck = json { { "ok", false }, { "error", { { "code", "timeout" }, { "message", "stuck" } } } };
            REQUIRE_FALSE (ChildProcessScanner::parseScanEnvelope (juce::String (stuck.dump()), found));
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

SCENARIO ("a host that started without a list file leaves another process's list alone", "[engine][effects][hosting]")
{
    MessageThread messageThread;
    TempFolder temp;

    GIVEN ("a host that found no list file, and a second one that scanned and saved meanwhile")
    {
        TestHost first (temp.folder, false);
        REQUIRE_FALSE (first.host.loadList());

        TestHost second (temp.folder);
        REQUIRE (second.host.saveList());

        WHEN ("the first, which changed nothing, saves what changed")
        {
            REQUIRE (first.host.saveListIfChanged());

            THEN ("the second's list is still in the file")
            {
                TestHost third (temp.folder, false);
                REQUIRE (third.host.loadList());
                REQUIRE (third.host.getList().getNumTypes() == 2);
            }
        }

        WHEN ("the first changes its own list")
        {
            first.host.getList().addToBlacklist ("/test/crashes.testa");

            THEN ("that is written")
            {
                REQUIRE (first.host.saveListIfChanged());
                TestHost third (temp.folder, false);
                REQUIRE (third.host.loadList());
                REQUIRE (third.host.getList().getBlacklistedFiles().contains ("/test/crashes.testa"));
            }
        }
    }
}

SCENARIO ("a host that is turned off offers no plugins", "[engine][effects][hosting][security]")
{
    MessageThread messageThread;
    TempFolder temp;
    TestHost test (temp.folder);
    auto& host = test.host;
    const auto plugin = TestPluginInstance::describe (gainIn ("TestA", 0x1111));

    GIVEN ("a host with plugins, attached to a registry")
    {
        auto registry = std::make_shared<EffectRegistry>();
        host.attach (registry, PluginHost::Mode::full);

        THEN ("it is on, and the plugins are types of the registry")
        {
            REQUIRE (host.isEnabled());
            REQUIRE (registry->hasType ("testa:00001111"));
            REQUIRE (registry->hasType ("testb:00002222"));
        }

        WHEN ("it is turned off")
        {
            host.setEnabled (false);

            THEN ("the types leave the registry, and the host says why")
            {
                REQUIRE_FALSE (registry->hasType ("testa:00001111"));
                REQUIRE_FALSE (registry->hasType ("testb:00002222"));
                REQUIRE (host.availabilityOf (plugin) == PluginHost::Availability::disabled);
                REQUIRE (PluginHost::describe (PluginHost::Availability::disabled).isNotEmpty());
                REQUIRE_FALSE (host.canCreate (plugin));
            }

            THEN ("nothing is loaded, and the list keeps its plugins")
            {
                const auto instancesBefore = TestPluginInstance::instances();
                juce::String error;
                REQUIRE (host.createInstance (plugin, 44100.0, 512, error) == nullptr);
                REQUIRE (error.isNotEmpty());
                REQUIRE (TestPluginInstance::instances() == instancesBefore);
                REQUIRE (host.getList().getNumTypes() > 0);
                REQUIRE_FALSE (host.findSubstitute ("Test Gain", "Audionaut Tests").has_value());
            }

            AND_WHEN ("it is turned on again")
            {
                host.setEnabled (true);

                THEN ("the plugins are back, and load")
                {
                    REQUIRE (registry->hasType ("testa:00001111"));
                    auto effect = registry->create ("testa:00001111");
                    auto* hosted = dynamic_cast<HostedEffect*> (effect.get());
                    REQUIRE (hosted != nullptr);
                    REQUIRE (hosted->isLoaded());
                }
            }
        }
    }
}

SCENARIO ("a plugin the system asks the user about is only there where that is allowed", "[engine][effects][hosting][security]")
{
    MessageThread messageThread;
    TempFolder temp;
    TestHost test (temp.folder);
    auto& host = test.host;
    const auto plain = TestPluginInstance::describe (gainIn ("TestA", 0x1111));
    const auto asking = TestPluginInstance::describe (gainIn ("TestB", 0x2222));

    GIVEN ("a host one of whose plugins needs lowered security settings, attached to a registry")
    {
        host.setLoweredSecurityQuestion ([] (const juce::PluginDescription& description) { return description.pluginFormatName == "TestB"; });
        auto registry = std::make_shared<EffectRegistry>();
        host.attach (registry, PluginHost::Mode::full);

        THEN ("it is not offered, and the host says why; the other plugin is")
        {
            REQUIRE_FALSE (host.isLoweredSecurityAllowed());
            REQUIRE (host.availabilityOf (asking) == PluginHost::Availability::needsLoweredSecurity);
            REQUIRE (PluginHost::describe (PluginHost::Availability::needsLoweredSecurity).isNotEmpty());
            REQUIRE_FALSE (host.canCreate (asking));
            REQUIRE_FALSE (registry->hasType ("testb:00002222"));
            REQUIRE (host.availabilityOf (plain) == PluginHost::Availability::available);
            REQUIRE (registry->hasType ("testa:00001111"));
        }

        THEN ("it is not loaded, and not taken as another format's stand-in")
        {
            const auto instancesBefore = TestPluginInstance::instances();
            juce::String error;
            REQUIRE (host.createInstance (asking, 44100.0, 512, error) == nullptr);
            REQUIRE (error.isNotEmpty());
            REQUIRE (TestPluginInstance::instances() == instancesBefore);

            host.getList().removeType (plain);
            REQUIRE_FALSE (host.findSubstitute ("Test Gain", "Audionaut Tests").has_value());
        }

        WHEN ("it is allowed")
        {
            host.setLoweredSecurityAllowed (true);

            THEN ("the plugin is a type of the attached registry, and loads")
            {
                REQUIRE (host.availabilityOf (asking) == PluginHost::Availability::available);
                REQUIRE (registry->hasType ("testb:00002222"));
                auto effect = registry->create ("testb:00002222");
                auto* hosted = dynamic_cast<HostedEffect*> (effect.get());
                REQUIRE (hosted != nullptr);
                REQUIRE (hosted->isLoaded());
            }

            AND_WHEN ("it is taken back")
            {
                host.setLoweredSecurityAllowed (false);

                THEN ("the type leaves the registry again")
                {
                    REQUIRE_FALSE (registry->hasType ("testb:00002222"));
                    REQUIRE (registry->hasType ("testa:00001111"));
                }
            }
        }
    }

    GIVEN ("what a format says when the user declined")
    {
        THEN ("the host says it in plain words, and leaves other messages alone")
        {
            const auto declined = PluginHost::describeLoadError ("An OS error occurred during initialisation of the plug-in (-128)");
            REQUIRE_FALSE (declined.contains ("-128"));
            REQUIRE (declined.containsIgnoreCase ("security settings"));
            REQUIRE (PluginHost::describeLoadError ("the file is damaged") == "the file is damaged");
            REQUIRE (PluginHost::describeLoadError ({}).isEmpty());
        }
    }
}

SCENARIO ("an engine waits for its own plugins, not for another engine's", "[engine][effects][hosting]")
{
    MessageThread messageThread;
    TempFolder temp;

    GIVEN ("one host with a plugin that loads asynchronously, attached to two registries")
    {
        PluginHost host (temp.folder, false);
        auto format = std::make_unique<TestPluginFormat> ("TestC", std::vector { gainIn ("TestC", 0x3333) });
        format->asyncOnly = true;
        auto* testFormat = format.get();
        host.getFormats().addFormat (std::move (format));
        host.getList().addType (TestPluginInstance::describe (gainIn ("TestC", 0x3333)));

        auto app = std::make_shared<EffectRegistry>();
        auto scratch = std::make_shared<EffectRegistry>();
        host.attach (app, PluginHost::Mode::full);
        host.attach (scratch, PluginHost::Mode::full);

        WHEN ("the first registry's plugin never arrives")
        {
            testFormat->withhold = true;
            auto stuck = app->create ("testc:00003333");
            messageThread.pump();   // the creation was asked for, and is withheld
            REQUIRE (host.getNumPendingLoads() == 1);

            THEN ("the second registry has nothing to wait for")
            {
                REQUIRE (PluginHost::waitForPendingLoads (*scratch, 2000));
                REQUIRE_FALSE (PluginHost::waitForPendingLoads (*app, 100));
            }

            AND_WHEN ("the second creates the plugin too, and that one arrives")
            {
                testFormat->withhold = false;
                auto effect = scratch->create ("testc:00003333");
                auto* hosted = dynamic_cast<HostedEffect*> (effect.get());
                REQUIRE (hosted != nullptr);
                REQUIRE (hosted->getStatus() == HostedEffect::Status::loading);

                THEN ("the second's wait ends with its plugin, the first's goes on")
                {
                    REQUIRE (PluginHost::waitForPendingLoads (*scratch, 5000));
                    REQUIRE (hosted->isLoaded());
                    REQUIRE (host.getNumPendingLoads() == 1);
                }
            }
        }

        WHEN ("a placeholder of a catalogue-only registry is given its plugin later")
        {
            auto catalogue = std::make_shared<EffectRegistry>();
            host.attach (catalogue, PluginHost::Mode::catalogueOnly);
            auto effect = catalogue->create ("testc:00003333");
            auto* hosted = dynamic_cast<HostedEffect*> (effect.get());
            REQUIRE (hosted != nullptr);
            REQUIRE (host.load (*hosted));

            THEN ("that load counts for its registry")
            {
                REQUIRE (catalogue->getPendingArrivals()->load() == 1);
                REQUIRE (PluginHost::waitForPendingLoads (*catalogue, 5000));
                REQUIRE (hosted->isLoaded());
            }
        }
    }
}

namespace
{
    /// A scan worker on a thread of this process: the real loop at the other end of the pipe, with a
    /// scan function of the test's. A scan that throws Dies ends the worker without an answer.
    struct Dies {};

    class ThreadWorker final : public ChildProcessScanner::Worker
    {
    public:
        ThreadWorker (ChildProcessScanner::ScanFunction scan_, int idleMs_) : scan (std::move (scan_)), idleMs (idleMs_) {}
        ~ThreadWorker() override
        {
            if (thread.joinable())
                thread.join();
        }

        bool start (const juce::String& pipeName) override
        {
            running = true;
            thread = std::thread ([this, pipeName]
            {
                // a killed worker ends at its next look at the pipe
                try { ChildProcessScanner::serve (pipeName, scan, idleMs, [this] { if (killed) throw Dies(); }); }
                catch (const Dies&) {}
                running = false;
            });
            return true;
        }

        bool isRunning() override { return running; }
        void kill() override { killed = true; }

    private:
        ChildProcessScanner::ScanFunction scan;
        int idleMs;
        std::atomic<bool> running { false };
        std::atomic<bool> killed { false };
        std::thread thread;
    };

    /// Started, alive, and silent.
    struct MuteWorker final : public ChildProcessScanner::Worker
    {
        bool start (const juce::String&) override { return true; }
        bool isRunning() override { return true; }
        void kill() override {}
    };

    json describedIn (const ChildProcessScanner::Request& request)
    {
        const auto& format = request.format;
        const auto& file = request.file;
        const auto timeoutMs = request.timeoutMs;

        if (format != "TestA")
            return { { "ok", false }, { "code", "unknown_format" } };
        if (file == "dies")
            throw Dies();
        if (file == "slow")
            juce::Thread::sleep (timeoutMs * 3);   // well past what the scanner waits, whatever that is

        const auto description = TestPluginInstance::describe (gainIn ("TestA", file.getIntValue()));
        const auto xml = description.createXml()->toString().toStdString();
        auto plugins = json::array ({ xml });
        if (file == "many")
            for (auto i = 0; i < 999; ++i)
                plugins.push_back (xml);   // an answer of several hundred kilobytes: more than a pipe holds at once
        return { { "ok", true }, { "plugins", plugins } };
    }

    /// Workers that die of "fragile" once they have loaded something else before it.
    ChildProcessScanner::WorkerFactory workersThatWearOut (int idleMs)
    {
        return [idleMs]
        {
            auto loaded = std::make_shared<int> (0);
            return std::make_unique<ThreadWorker> ([loaded] (const ChildProcessScanner::Request& request)
            {
                if (request.file == "fragile" && *loaded > 0)
                    throw Dies();
                ++*loaded;
                return describedIn (request);
            }, idleMs);
        };
    }

    /// Workers that find nothing in a "late" file once they have loaded something else before it, and
    /// nothing in "empty" ever.
    ChildProcessScanner::WorkerFactory workersThatGoBlind (int idleMs)
    {
        return [idleMs]
        {
            auto loaded = std::make_shared<int> (0);
            return std::make_unique<ThreadWorker> ([loaded] (const ChildProcessScanner::Request& request) -> json
            {
                const auto blind = request.file.startsWith ("late") && *loaded > 0;
                ++*loaded;
                if (blind || request.file == "empty")
                    return { { "ok", true }, { "plugins", json::array() } };
                return describedIn (request);
            }, idleMs);
        };
    }

    // Generous, so that a busy machine does not decide a test: the scenarios that are about time
    // running out set their own.
    constexpr auto patientIdleMs = 20000;
    constexpr auto patientTimeoutMs = 20000;
}

SCENARIO ("the child-process scanner asks a new worker about a file the used one found nothing in", "[engine][effects][hosting][scan]")
{
    TestPluginFormat format ("TestA", std::vector { gainIn ("TestA", 0x1111) });

    GIVEN ("workers that find nothing in some files once they have loaded another")
    {
        ChildProcessScanner scanner (workersThatGoBlind (patientIdleMs), patientTimeoutMs);
        juce::OwnedArray<juce::PluginDescription> found;

        WHEN ("the system may ask the user about plugins")
        {
            scanner.setLoweredSecurityFlag (std::make_shared<const std::atomic<bool>> (true));
            REQUIRE (scanner.findPluginTypesFor (format, found, "11"));
            REQUIRE (scanner.findPluginTypesFor (format, found, "late 22"));

            THEN ("a new worker found the plugin, and goes on with the next files")
            {
                REQUIRE (found.size() == 2);
                REQUIRE (scanner.getNumWorkersStarted() == 2);
                REQUIRE (scanner.findPluginTypesFor (format, found, "33"));
                REQUIRE (scanner.getNumWorkersStarted() == 2);
            }

            AND_WHEN ("files are empty in a new worker too")
            {
                REQUIRE (scanner.findPluginTypesFor (format, found, "empty"));
                REQUIRE (scanner.findPluginTypesFor (format, found, "44"));
                REQUIRE (scanner.findPluginTypesFor (format, found, "empty"));
                const auto started = scanner.getNumWorkersStarted();
                REQUIRE (started == 4);

                THEN ("after two of them no further worker is started for an empty file")
                {
                    REQUIRE (scanner.findPluginTypesFor (format, found, "55"));
                    REQUIRE (scanner.findPluginTypesFor (format, found, "empty"));
                    REQUIRE (scanner.getNumWorkersStarted() == started);
                    REQUIRE (found.size() == 4);
                }
            }
        }

        WHEN ("it may not")
        {
            REQUIRE (scanner.findPluginTypesFor (format, found, "11"));
            REQUIRE (scanner.findPluginTypesFor (format, found, "late 22"));

            THEN ("the file counts as empty and the worker stays")
            {
                REQUIRE (found.size() == 1);
                REQUIRE (scanner.getNumWorkersStarted() == 1);
            }
        }
    }
}

SCENARIO ("the child-process scanner says what it did with each file", "[engine][effects][hosting][scan]")
{
    TestPluginFormat format ("TestA", std::vector { gainIn ("TestA", 0x1111) });

    GIVEN ("a scanner with a log")
    {
        ChildProcessScanner scanner ([] { return std::make_unique<ThreadWorker> (describedIn, patientIdleMs); }, patientTimeoutMs);
        juce::StringArray lines;
        scanner.setLog ([&lines] (const juce::String& line) { lines.add (line); });

        WHEN ("a file is found and another takes its worker down")
        {
            juce::OwnedArray<juce::PluginDescription> found;
            REQUIRE (scanner.findPluginTypesFor (format, found, "11"));
            REQUIRE_FALSE (scanner.findPluginTypesFor (format, found, "dies"));

            THEN ("the log has the workers, the plugin by name and the failure with its reason")
            {
                const auto log = lines.joinIntoString ("\n");
                INFO (log);
                REQUIRE (lines[0].startsWith ("worker 1 started"));
                REQUIRE (lines[1].startsWith ("TestA  11  ->  1 found: " + found[0]->name));
                REQUIRE (log.contains ("asked again of a new worker"));
                REQUIRE (log.contains ("worker 2 started"));
                REQUIRE (lines[lines.size() - 1].startsWith ("TestA  dies  ->  FAILED, file skipped: the worker process ended"));
            }
        }
    }
}

SCENARIO ("the child-process scanner asks one worker for many files", "[engine][effects][hosting][scan]")
{
    TestPluginFormat format ("TestA", std::vector { gainIn ("TestA", 0x1111) });
    TestPluginFormat other ("TestB", std::vector { gainIn ("TestB", 0x2222) });

    GIVEN ("a scanner whose workers serve the pipe")
    {
        ChildProcessScanner scanner ([] { return std::make_unique<ThreadWorker> (describedIn, patientIdleMs); }, patientTimeoutMs);

        WHEN ("three files are scanned in a row")
        {
            juce::OwnedArray<juce::PluginDescription> found;
            REQUIRE (scanner.findPluginTypesFor (format, found, "11"));
            REQUIRE (scanner.findPluginTypesFor (format, found, "22"));
            REQUIRE (scanner.findPluginTypesFor (format, found, "33"));

            THEN ("one worker answered them all")
            {
                REQUIRE (scanner.getNumWorkersStarted() == 1);
                REQUIRE (found.size() == 3);
                REQUIRE (found[0]->uniqueId == 11);
                REQUIRE (found[2]->uniqueId == 33);
            }
        }

        WHEN ("a file takes the worker down")
        {
            juce::OwnedArray<juce::PluginDescription> found;
            REQUIRE (scanner.findPluginTypesFor (format, found, "11"));
            const auto verdict = scanner.findPluginTypesFor (format, found, "dies");

            THEN ("that file failed in a worker of its own too, and the next one gets a new worker")
            {
                REQUIRE_FALSE (verdict);
                REQUIRE (scanner.getNumWorkersStarted() == 2);   // the one it shared, and one to itself
                REQUIRE (scanner.findPluginTypesFor (format, found, "22"));
                REQUIRE (scanner.getNumWorkersStarted() == 3);
                REQUIRE (found.size() == 2);
            }
        }

        WHEN ("a file takes the first worker down that sees it")
        {
            juce::OwnedArray<juce::PluginDescription> found;
            const auto verdict = scanner.findPluginTypesFor (format, found, "dies");

            THEN ("it failed, and is not asked again: nothing else was in that worker")
            {
                REQUIRE_FALSE (verdict);
                REQUIRE (scanner.getNumWorkersStarted() == 1);
            }
        }

        WHEN ("a file's answer is larger than the pipe holds at once")
        {
            juce::OwnedArray<juce::PluginDescription> found;
            const auto verdict = scanner.findPluginTypesFor (format, found, "many");

            THEN ("it arrives whole")
            {
                REQUIRE (verdict);
                REQUIRE (found.size() == 1000);
                REQUIRE (scanner.findPluginTypesFor (format, found, "22"));
                REQUIRE (scanner.getNumWorkersStarted() == 1);
            }
        }

        WHEN ("the worker has no such format")
        {
            juce::OwnedArray<juce::PluginDescription> found;
            const auto verdict = scanner.findPluginTypesFor (other, found, "11");

            THEN ("the file has nothing, and is no crash")
            {
                REQUIRE (verdict);
                REQUIRE (found.isEmpty());
            }
        }
    }

    GIVEN ("workers that die of a file once they have loaded another")
    {
        ChildProcessScanner scanner (workersThatWearOut (patientIdleMs), patientTimeoutMs);

        WHEN ("that file comes second")
        {
            juce::OwnedArray<juce::PluginDescription> found;
            REQUIRE (scanner.findPluginTypesFor (format, found, "11"));
            const auto verdict = scanner.findPluginTypesFor (format, found, "fragile");

            THEN ("it is asked of a new worker, and what the first one had loaded is not held against it")
            {
                REQUIRE (verdict);
                REQUIRE (scanner.getNumWorkersStarted() == 2);
                REQUIRE (found.size() == 2);
            }
        }
    }

    GIVEN ("a scanner that is told whether plugins may ask for lowered security settings")
    {
        auto heard = std::make_shared<std::vector<bool>>();
        ChildProcessScanner scanner ([heard]
        {
            return std::make_unique<ThreadWorker> ([heard] (const ChildProcessScanner::Request& request)
            {
                heard->push_back (request.loweredSecurity);
                return describedIn (request);
            }, patientIdleMs);
        }, patientTimeoutMs);
        auto allowed = std::make_shared<std::atomic<bool>> (false);

        WHEN ("files are scanned before and after it is allowed")
        {
            juce::OwnedArray<juce::PluginDescription> found;
            REQUIRE (scanner.findPluginTypesFor (format, found, "11"));   // nothing said: no
            scanner.setLoweredSecurityFlag (allowed);
            REQUIRE (scanner.findPluginTypesFor (format, found, "22"));
            allowed->store (true);
            REQUIRE (scanner.findPluginTypesFor (format, found, "33"));

            THEN ("the worker hears it with every file")
            {
                REQUIRE (*heard == std::vector<bool> { false, false, true });
            }
        }
    }

    GIVEN ("a scanner that does not wait long")
    {
        ChildProcessScanner scanner ([] { return std::make_unique<ThreadWorker> (describedIn, patientIdleMs); }, 500);

        WHEN ("a file keeps the worker longer than that")
        {
            juce::OwnedArray<juce::PluginDescription> found;
            const auto verdict = scanner.findPluginTypesFor (format, found, "slow");

            THEN ("that file failed")
            {
                REQUIRE_FALSE (verdict);
                REQUIRE (found.isEmpty());
                REQUIRE (scanner.getNumWorkersStarted() == 1);
            }
        }
    }

    GIVEN ("workers that leave soon when nobody asks")
    {
        ChildProcessScanner scanner ([] { return std::make_unique<ThreadWorker> (describedIn, 200); }, patientTimeoutMs);

        WHEN ("the worker was left alone until it went")
        {
            juce::OwnedArray<juce::PluginDescription> found;
            REQUIRE (scanner.findPluginTypesFor (format, found, "11"));
            const auto before = scanner.getNumWorkersStarted();
            juce::Thread::sleep (1500);

            THEN ("the next file gets a new one, and its answer")
            {
                REQUIRE (scanner.findPluginTypesFor (format, found, "22"));
                REQUIRE (scanner.getNumWorkersStarted() > before);
                REQUIRE (found.size() == 2);
            }
        }
    }

    GIVEN ("a scanner whose worker never says hello")
    {
        ChildProcessScanner scanner ([] { return std::make_unique<MuteWorker>(); }, patientTimeoutMs, 200);

        WHEN ("files are scanned")
        {
            juce::OwnedArray<juce::PluginDescription> found;
            const auto first = scanner.findPluginTypesFor (format, found, "11");
            const auto second = scanner.findPluginTypesFor (format, found, "22");

            THEN ("the pipe is given up after the first, with nothing to fall back to here")
            {
                REQUIRE_FALSE (first);
                REQUIRE_FALSE (second);
                REQUIRE (scanner.getNumWorkersStarted() == 1);
            }
        }
    }
}
