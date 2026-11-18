//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Engine/Effects/EffectRegistry.h"
#include "Engine/Effects/Hosting/AudioUnitRegistry.h"
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

SCENARIO ("the plugin host scans extra folders for the formats that look in folders", "[engine][effects][hosting]")
{
    MessageThread messageThread;
    TempFolder temp;

    GIVEN ("a host whose formats look in no folders")
    {
        TestHost test (temp.folder, false);
        auto& host = test.host;
        auto* inFolders = dynamic_cast<TestPluginFormat*> (host.getFormats().getFormat (0));
        auto* byIdentifier = host.getFormats().getFormat (1);
        REQUIRE (inFolders != nullptr);
        const auto standard = temp.folder.getChildFile ("standard");
        const auto extra = temp.folder.getChildFile ("extra");

        THEN ("extra folders mean nothing to it, and there are none")
        {
            REQUIRE_FALSE (host.scansFolders());
            REQUIRE (host.getExtraScanFolders().getNumPaths() == 0);
        }

        WHEN ("one format has a standard folder and an extra one is set")
        {
            inFolders->defaultLocations.add (standard);
            REQUIRE (host.setExtraScanFolders (juce::FileSearchPath (extra.getFullPathName())));

            THEN ("that format's scan looks in both, the other format's in none")
            {
                REQUIRE (host.scansFolders());
                const auto folders = host.getScanFolders (*inFolders);
                REQUIRE (folders.getNumPaths() == 2);
                REQUIRE (folders[0] == standard);
                REQUIRE (folders[1] == extra);
                REQUIRE (host.getScanFolders (*byIdentifier).getNumPaths() == 0);
            }

            THEN ("another process's host on the same data folder has the extra folder too")
            {
                PluginHost other (temp.folder, false);
                REQUIRE (other.getExtraScanFolders().getNumPaths() == 1);
                REQUIRE (other.getExtraScanFolders()[0] == extra);
            }

            AND_WHEN ("the extra folders are taken away again")
            {
                REQUIRE (host.setExtraScanFolders ({}));

                THEN ("the scan looks in the standard folder only")
                {
                    REQUIRE (host.getExtraScanFolders().getNumPaths() == 0);
                    REQUIRE (host.getScanFolders (*inFolders).getNumPaths() == 1);
                }
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

SCENARIO ("the plugin host lists what the system knows to be installed without a scan", "[engine][effects][hosting][installed]")
{
    MessageThread messageThread;
    TempFolder temp;

    const auto named = [] (const juce::String& name, int uid)
    {
        auto options = gainIn ("TestA", uid);
        options.name = name;
        return options;
    };
    const auto one = TestPluginInstance::describe (named ("One", 0x0101));
    const auto two = TestPluginInstance::describe (named ("Two", 0x0202));
    const auto ghost = TestPluginInstance::describe (named ("Ghost", 0x0303));   // installed, but no scan finds it

    GIVEN ("a host whose system lists three plugins, two of which its format can load")
    {
        PluginHost host (temp.folder, false);
        host.getFormats().addFormat (std::make_unique<TestPluginFormat> ("TestA", std::vector { named ("One", 0x0101), named ("Two", 0x0202) }));
        auto installed = std::make_shared<std::vector<juce::PluginDescription>> (std::vector { one, two, ghost });
        host.setInstalledSource ([installed] { return *installed; });
        auto registry = std::make_shared<EffectRegistry>();

        WHEN ("the list is brought in line with it")
        {
            REQUIRE (host.syncWithInstalled());

            THEN ("all three are listed as not scanned, and offered as inserts")
            {
                REQUIRE (host.getList().getNumTypes() == 3);
                for (const auto& description : host.getList().getTypes())
                    REQUIRE_FALSE (PluginHost::isScanned (description));
                host.registerInto (*registry, PluginHost::Mode::full);
                REQUIRE (registry->hasType ("testa:00000101"));
                REQUIRE (registry->hasType ("testa:00000303"));
                REQUIRE_FALSE (host.syncWithInstalled());   // nothing more to do
            }

            THEN ("a host that reads the saved list has them as not scanned too")
            {
                REQUIRE (host.saveList());
                PluginHost second (temp.folder, false);
                REQUIRE (second.loadList());
                REQUIRE (second.getList().getNumTypes() == 3);
                REQUIRE_FALSE (PluginHost::isScanned (second.getList().getTypes()[0]));
            }

            AND_WHEN ("one of them is no longer installed")
            {
                installed->pop_back();
                REQUIRE (host.syncWithInstalled());

                THEN ("it leaves the list")
                {
                    REQUIRE (host.getList().getNumTypes() == 2);
                    REQUIRE_FALSE (host.findByTypeId ("testa:00000303").has_value());
                }
            }

            AND_WHEN ("the host scans")
            {
                const auto result = host.scanAll();

                THEN ("what the scan loaded is scanned, the rest is listed as before")
                {
                    REQUIRE (result.added == 2);
                    REQUIRE (result.found == 2);
                    REQUIRE (host.getList().getNumTypes() == 3);
                    REQUIRE (PluginHost::isScanned (*host.findByTypeId ("testa:00000101")));
                    REQUIRE (PluginHost::isScanned (*host.findByTypeId ("testa:00000202")));
                    REQUIRE_FALSE (PluginHost::isScanned (*host.findByTypeId ("testa:00000303")));
                }

                AND_WHEN ("the list is brought in line again")
                {
                    THEN ("the scanned entries stay as the scan left them")
                    {
                        REQUIRE_FALSE (host.syncWithInstalled());
                        REQUIRE (PluginHost::isScanned (*host.findByTypeId ("testa:00000101")));
                    }
                }
            }
        }

        WHEN ("a scan's worker loaded one of them and found no plugin in it")
        {
            host.syncWithInstalled();
            host.noteLoadedEmpty (ghost.fileOrIdentifier);
            host.finishScan();

            THEN ("it is skipped from now on, and not listed as installed again")
            {
                REQUIRE (host.getList().getBlacklistedFiles().contains (ghost.fileOrIdentifier));
                REQUIRE_FALSE (host.findByTypeId ("testa:00000303").has_value());
                REQUIRE_FALSE (host.syncWithInstalled());
                REQUIRE (host.getList().getNumTypes() == 2);
            }
        }

        WHEN ("a plugin that is listed without a scan comes in a new version")
        {
            host.syncWithInstalled();
            (*installed)[0].version = "2.0";
            REQUIRE (host.syncWithInstalled());

            THEN ("its entry says so, and is still not scanned")
            {
                REQUIRE (host.findByTypeId ("testa:00000101")->version == "2.0");
                REQUIRE_FALSE (PluginHost::isScanned (*host.findByTypeId ("testa:00000101")));
                REQUIRE (host.getList().getNumTypes() == 3);
            }
        }

        WHEN ("one of them is a file a scan gave up on")
        {
            host.getList().addToBlacklist (two.fileOrIdentifier);
            host.syncWithInstalled();

            THEN ("it stays skipped, and is not listed")
            {
                REQUIRE (host.getList().getNumTypes() == 2);
                REQUIRE_FALSE (host.findByTypeId ("testa:00000202").has_value());
            }
        }
    }
}

SCENARIO ("a not-scanned plugin that runs in the host's process is checked by the scanner before its first load", "[engine][effects][hosting][installed]")
{
    MessageThread messageThread;
    TempFolder temp;

    /// Stands in for the worker process: counts what it is asked, and takes a plugin for one that crashes when told to.
    struct CountingScanner final : public juce::KnownPluginList::CustomScanner
    {
        CountingScanner (std::shared_ptr<std::atomic<int>> asked_, std::shared_ptr<std::atomic<bool>> crashes_) :
            asked (std::move (asked_)), crashes (std::move (crashes_)) {}

        bool findPluginTypesFor (juce::AudioPluginFormat& format, juce::OwnedArray<juce::PluginDescription>& result,
                                 const juce::String& fileOrIdentifier) override
        {
            ++*asked;
            if (crashes->load())
                return false;
            format.findAllTypesForFile (result, fileOrIdentifier);
            return true;
        }

        std::shared_ptr<std::atomic<int>> asked;
        std::shared_ptr<std::atomic<bool>> crashes;
    };

    /// A scanner whose worker does not run: every file comes back without a verdict, and without a plugin.
    struct NoWorkerScanner final : public juce::KnownPluginList::CustomScanner
    {
        bool findPluginTypesFor (juce::AudioPluginFormat&, juce::OwnedArray<juce::PluginDescription>&, const juce::String&) override { return true; }
    };

    GIVEN ("a host that lists an installed plugin without a scan, and checks before a first load")
    {
        PluginHost host (temp.folder, false);
        host.getFormats().addFormat (std::make_unique<TestPluginFormat> ("TestA", std::vector { gainIn ("TestA", 0x1111) }));
        const auto plugin = TestPluginInstance::describe (gainIn ("TestA", 0x1111));
        host.setInstalledSource ([plugin] { return std::vector { plugin }; });
        auto asked = std::make_shared<std::atomic<int>> (0);
        auto crashes = std::make_shared<std::atomic<bool>> (false);
        host.getList().setCustomScanner (std::make_unique<CountingScanner> (asked, crashes));
        host.setChecksBeforeFirstLoad (true);
        host.syncWithInstalled();
        auto registry = std::make_shared<EffectRegistry>();
        host.attach (registry, PluginHost::Mode::full);

        WHEN ("it is created as an effect")
        {
            auto effect = registry->create ("testa:00001111");
            auto* hosted = dynamic_cast<HostedEffect*> (effect.get());
            REQUIRE (hosted != nullptr);

            THEN ("it waits as a loading placeholder, is scanned once, and loads")
            {
                REQUIRE (hosted->getStatus() == HostedEffect::Status::loading);
                REQUIRE (host.getNumPendingLoads() == 1);
                REQUIRE (host.waitForPendingLoads (5000));
                REQUIRE (asked->load() == 1);
                REQUIRE (hosted->isLoaded());
                REQUIRE (PluginHost::isScanned (*host.findByTypeId ("testa:00001111")));
            }

            AND_WHEN ("it is created again")
            {
                REQUIRE (host.waitForPendingLoads (5000));
                messageThread.pump();   // the registry hears of the scanned entry
                auto again = registry->create ("testa:00001111");

                THEN ("it loads at once, without another check")
                {
                    REQUIRE (dynamic_cast<HostedEffect*> (again.get())->isLoaded());
                    REQUIRE (asked->load() == 1);
                }
            }
        }

        WHEN ("it takes the scanner's worker down")
        {
            crashes->store (true);
            auto effect = registry->create ("testa:00001111");
            auto* hosted = dynamic_cast<HostedEffect*> (effect.get());
            REQUIRE (host.waitForPendingLoads (5000));

            THEN ("it is not loaded here, is skipped from now on and leaves the list")
            {
                REQUIRE (hosted->getStatus() == HostedEffect::Status::notLoaded);
                REQUIRE (hosted->getError().contains ("skipped"));
                REQUIRE (host.getList().getBlacklistedFiles().contains (plugin.fileOrIdentifier));
                REQUIRE (host.getList().getNumTypes() == 0);
                REQUIRE_FALSE (host.syncWithInstalled());   // and does not come back as not scanned
            }
        }

        WHEN ("the scanner's worker does not run")
        {
            host.getList().setCustomScanner (std::make_unique<NoWorkerScanner>());
            auto effect = registry->create ("testa:00001111");
            REQUIRE (host.waitForPendingLoads (5000));

            THEN ("the plugin loads as it would without a check, and is not held to be scanned")
            {
                REQUIRE (dynamic_cast<HostedEffect*> (effect.get())->isLoaded());
                REQUIRE (host.getList().getBlacklistedFiles().isEmpty());
                REQUIRE_FALSE (PluginHost::isScanned (*host.findByTypeId ("testa:00001111")));
            }
        }

        WHEN ("the worker loads it and finds no plugin in it")
        {
            host.getList().setCustomScanner (std::make_unique<NoWorkerScanner>());
            host.noteLoadedEmpty (plugin.fileOrIdentifier);
            auto effect = registry->create ("testa:00001111");
            REQUIRE (host.waitForPendingLoads (5000));

            THEN ("it is not loaded here, and skipped from now on")
            {
                REQUIRE (dynamic_cast<HostedEffect*> (effect.get())->getStatus() == HostedEffect::Status::notLoaded);
                REQUIRE (host.getList().getBlacklistedFiles().contains (plugin.fileOrIdentifier));
                REQUIRE (host.getList().getNumTypes() == 0);
            }
        }

        WHEN ("the effect is gone before the check is done")
        {
            auto effect = registry->create ("testa:00001111");
            effect.reset();

            THEN ("nothing is left pending")
            {
                REQUIRE (host.waitForPendingLoads (5000));
                REQUIRE (host.getNumPendingLoads() == 0);
            }
        }
    }
}

SCENARIO ("plugins the system asks the user about are loaded one at a time, and a no answers for those waiting", "[engine][effects][hosting][security]")
{
    MessageThread messageThread;
    TempFolder temp;

    const auto named = [] (const juce::String& name, int uid)
    {
        auto options = gainIn ("TestC", uid);
        options.name = name;
        return options;
    };

    GIVEN ("a host with three such plugins, which load in a process of their own")
    {
        PluginHost host (temp.folder, false);
        auto owned = std::make_unique<TestPluginFormat> ("TestC", std::vector { named ("One", 0x0101), named ("Two", 0x0202), named ("Three", 0x0303) });
        auto* format = owned.get();
        format->asyncOnly = true;
        auto asked = std::make_shared<int> (0);
        format->onCreate = [asked] { ++*asked; };
        host.getFormats().addFormat (std::move (owned));
        for (const auto& plugin : { named ("One", 0x0101), named ("Two", 0x0202), named ("Three", 0x0303) })
            host.getList().addType (TestPluginInstance::describe (plugin));
        host.setLoweredSecurityQuestion ([] (const juce::PluginDescription& description) { return description.pluginFormatName == "TestC"; });
        host.setLoweredSecurityAllowed (true);
        auto registry = std::make_shared<EffectRegistry>();
        host.registerInto (*registry, PluginHost::Mode::full);

        WHEN ("all three are created and the user declines the question")
        {
            format->refuseWith = "An OS error occurred during initialisation of the plug-in (-128)";
            auto first = registry->create ("testc:00000101");
            auto second = registry->create ("testc:00000202");
            auto third = registry->create ("testc:00000303");
            REQUIRE (host.getNumPendingLoads() == 3);
            REQUIRE (host.waitForPendingLoads (5000));

            THEN ("the question came up once, and none of them is loaded")
            {
                REQUIRE (*asked == 1);
                for (auto* effect : { first.get(), second.get(), third.get() })
                {
                    auto* hosted = dynamic_cast<HostedEffect*> (effect);
                    REQUIRE (hosted != nullptr);
                    REQUIRE (hosted->getStatus() == HostedEffect::Status::notLoaded);
                    REQUIRE (hosted->getError().contains ("declined"));
                }
            }

            AND_WHEN ("one is created again and the user agrees")
            {
                format->refuseWith.clear();
                auto again = registry->create ("testc:00000101");
                REQUIRE (host.waitForPendingLoads (5000));

                THEN ("it was asked for, and loads")
                {
                    REQUIRE (*asked == 2);
                    REQUIRE (dynamic_cast<HostedEffect*> (again.get())->isLoaded());
                }
            }
        }

        WHEN ("all three are created and the user agrees")
        {
            auto first = registry->create ("testc:00000101");
            auto second = registry->create ("testc:00000202");
            auto third = registry->create ("testc:00000303");
            REQUIRE (host.waitForPendingLoads (5000));

            THEN ("each one loads")
            {
                REQUIRE (*asked == 3);
                for (auto* effect : { first.get(), second.get(), third.get() })
                    REQUIRE (dynamic_cast<HostedEffect*> (effect)->isLoaded());
            }
        }

        WHEN ("the host goes while one is on its way and others wait")
        {
            auto short_lived = std::make_unique<PluginHost> (temp.folder.getChildFile ("second"), false);
            auto withheld = std::make_unique<TestPluginFormat> ("TestC", std::vector { named ("One", 0x0101), named ("Two", 0x0202) });
            withheld->asyncOnly = true;
            withheld->withhold = true;   // the system's question stays open
            short_lived->getFormats().addFormat (std::move (withheld));
            for (const auto& plugin : { named ("One", 0x0101), named ("Two", 0x0202) })
                short_lived->getList().addType (TestPluginInstance::describe (plugin));
            short_lived->setLoweredSecurityQuestion ([] (const juce::PluginDescription&) { return true; });
            short_lived->setLoweredSecurityAllowed (true);
            auto second = std::make_shared<EffectRegistry>();
            short_lived->registerInto (*second, PluginHost::Mode::full);
            auto first = second->create ("testc:00000101");
            auto waiting = second->create ("testc:00000202");
            REQUIRE (second->getPendingArrivals()->load() == 2);
            short_lived.reset();

            THEN ("the one that waited is not left waiting")
            {
                REQUIRE (dynamic_cast<HostedEffect*> (waiting.get())->getStatus() == HostedEffect::Status::notLoaded);
                REQUIRE (second->getPendingArrivals()->load() == 1);   // the one the system still holds
            }
        }

        WHEN ("one that waits its turn is gone before it comes")
        {
            format->refuseWith = "An OS error occurred during initialisation of the plug-in (-128)";
            auto first = registry->create ("testc:00000101");
            auto second = registry->create ("testc:00000202");
            second.reset();

            THEN ("nothing is left pending")
            {
                REQUIRE (host.waitForPendingLoads (5000));
                REQUIRE (host.getNumPendingLoads() == 0);
            }
        }
    }
}

#if JUCE_MAC
SCENARIO ("the system's list of Audio Units describes a plugin as a scan of it does", "[engine][effects][hosting][installed][audiounit]")
{
    MessageThread messageThread;

    GIVEN ("the platform's formats")
    {
        juce::AudioPluginFormatManager formats;
        PluginHost::addPlatformFormats (formats);

        WHEN ("the installed Audio Units are listed")
        {
            const auto started = juce::Time::getMillisecondCounterHiRes();
            const auto installed = AudioUnitRegistry::installed (formats);
            WARN ("listed " << installed.size() << " Audio Units in " << juce::Time::getMillisecondCounterHiRes() - started << " ms");

            THEN ("Apple's delay is among them, as its scan describes it but for the scan's time and its channels")
            {
                const auto delay = std::find_if (installed.begin(), installed.end(), [] (const juce::PluginDescription& description)
                {
                    return description.fileOrIdentifier == "AudioUnit:Effects/aufx,dely,appl";
                });
                REQUIRE (delay != installed.end());
                REQUIRE_FALSE (PluginHost::isScanned (*delay));

                juce::OwnedArray<juce::PluginDescription> scanned;
                formats.getFormat (0)->findAllTypesForFile (scanned, delay->fileOrIdentifier);
                REQUIRE (scanned.size() == 1);
                REQUIRE (PluginHost::isScanned (*scanned[0]));
                REQUIRE (delay->isDuplicateOf (*scanned[0]));
                REQUIRE (delay->name == scanned[0]->name);
                REQUIRE (delay->manufacturerName == scanned[0]->manufacturerName);
                REQUIRE (delay->version == scanned[0]->version);
                REQUIRE (delay->category == scanned[0]->category);
                REQUIRE (delay->pluginFormatName == scanned[0]->pluginFormatName);
                REQUIRE (delay->isInstrument == scanned[0]->isInstrument);
                REQUIRE (PluginTypeId::make (*delay) == PluginTypeId::make (*scanned[0]));

                // a scan loads the entry that is only listed, and leaves the scanned one alone
                REQUIRE (formats.getFormat (0)->pluginNeedsRescanning (*delay));
                REQUIRE_FALSE (formats.getFormat (0)->pluginNeedsRescanning (*scanned[0]));
            }
        }
    }
}
#endif

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
                REQUIRE (scanner.findPluginTypesFor (format, found, "empty"));
                const auto started = scanner.getNumWorkersStarted();
                REQUIRE (started == 4);

                THEN ("after two of them in a row no further worker is started for an empty file")
                {
                    REQUIRE (scanner.findPluginTypesFor (format, found, "empty"));
                    REQUIRE (scanner.getNumWorkersStarted() == started);
                    REQUIRE (found.size() == 2);

                    AND_THEN ("a file with a plugin in it does not start the count again, the next scan does")
                    {
                        REQUIRE (scanner.findPluginTypesFor (format, found, "55"));
                        REQUIRE (scanner.findPluginTypesFor (format, found, "empty"));
                        REQUIRE (scanner.getNumWorkersStarted() == started);

                        scanner.scanFinished();
                        REQUIRE (scanner.findPluginTypesFor (format, found, "empty"));
                        REQUIRE (scanner.getNumWorkersStarted() == started + 1);
                        REQUIRE (found.size() == 3);
                    }
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

SCENARIO ("the child-process scanner holds a worker that does not run against no file", "[engine][effects][hosting][scan]")
{
    TestPluginFormat format ("TestA", std::vector { gainIn ("TestA", 0x1111) });

    GIVEN ("a scanner whose worker executable is not there")
    {
        ChildProcessScanner scanner (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("audionaut-no-such-worker"), 5000);
        juce::StringArray lines;
        scanner.setLog ([&lines] (const juce::String& line) { lines.add (line); });

        WHEN ("files are scanned")
        {
            juce::OwnedArray<juce::PluginDescription> found;
            const auto first = scanner.findPluginTypesFor (format, found, "11");
            const auto second = scanner.findPluginTypesFor (format, found, "22");
            const auto third = scanner.findPluginTypesFor (format, found, "33");

            THEN ("nothing is found, no file is to be skipped, and the log says why")
            {
                const auto log = lines.joinIntoString ("\n");
                INFO (log);
                REQUIRE (first);
                REQUIRE (second);
                REQUIRE (third);
                REQUIRE (found.isEmpty());
                REQUIRE (log.contains ("TestA  33  ->  NOT SCANNED"));
            }
        }
    }
}

SCENARIO ("the child-process scanner does not skip a plugin for the first wait for the user's answer", "[engine][effects][hosting][scan]")
{
    TestPluginFormat format ("TestA", std::vector { gainIn ("TestA", 0x1111) });

    GIVEN ("workers whose plugin runs out of time while the system may be asking the user")
    {
        ChildProcessScanner scanner ([]
        {
            return std::make_unique<ThreadWorker> ([] (const ChildProcessScanner::Request& request) -> json
            {
                if (request.file == "asks")
                    return { { "ok", false }, { "code", "awaiting_consent" }, { "message", "waited for the answer" }, { "last", true } };
                return describedIn (request);
            }, patientIdleMs);
        }, patientTimeoutMs);
        juce::StringArray lines;
        scanner.setLog ([&lines] (const juce::String& line) { lines.add (line); });
        juce::OwnedArray<juce::PluginDescription> found;

        WHEN ("such a file is scanned")
        {
            REQUIRE (scanner.findPluginTypesFor (format, found, "11"));
            const auto first = scanner.findPluginTypesFor (format, found, "asks");

            THEN ("it is not to be skipped, the log says why, and the next file gets a new worker")
            {
                INFO (lines.joinIntoString ("\n"));
                REQUIRE (first);
                REQUIRE (found.size() == 1);
                REQUIRE (lines[lines.size() - 1].startsWith ("TestA  asks  ->  NOT SCANNED: waited for the answer"));
                REQUIRE (scanner.findPluginTypesFor (format, found, "22"));
                REQUIRE (found.size() == 2);
                REQUIRE (scanner.getNumWorkersStarted() == 2);
            }

            AND_WHEN ("it runs out of time again")
            {
                const auto second = scanner.findPluginTypesFor (format, found, "asks");

                THEN ("it is skipped from now on")
                {
                    REQUIRE_FALSE (second);
                    REQUIRE (lines[lines.size() - 1].startsWith ("TestA  asks  ->  FAILED, file skipped: waited for the answer"));
                }
            }
        }
    }
}

SCENARIO ("the child-process scanner has the system's question raised once per scan when the user declines it", "[engine][effects][hosting][scan]")
{
    TestPluginFormat format ("TestA", std::vector { gainIn ("TestA", 0x1111) });

    GIVEN ("workers whose user declines the question, and a scanner that may raise it")
    {
        auto raised = std::make_shared<std::atomic<int>> (0);
        ChildProcessScanner scanner ([raised]
        {
            return std::make_unique<ThreadWorker> ([raised] (const ChildProcessScanner::Request& request) -> json
            {
                if (! request.file.startsWith ("asks"))
                    return describedIn (request);
                if (! request.loweredSecurity)
                    return { { "ok", true }, { "plugins", json::array() } };   // not loaded, nobody asked
                ++*raised;
                return { { "ok", false }, { "code", "consent_declined" }, { "message", "declined" } };
            }, patientIdleMs);
        }, patientTimeoutMs);
        scanner.setLoweredSecurityFlag (std::make_shared<const std::atomic<bool>> (true));
        juce::StringArray lines;
        scanner.setLog ([&lines] (const juce::String& line) { lines.add (line); });
        juce::OwnedArray<juce::PluginDescription> found;

        WHEN ("several such files are scanned")
        {
            REQUIRE (scanner.findPluginTypesFor (format, found, "11"));
            const auto first = scanner.findPluginTypesFor (format, found, "asks 1");
            const auto second = scanner.findPluginTypesFor (format, found, "asks 2");
            const auto third = scanner.findPluginTypesFor (format, found, "asks 3");

            THEN ("the question came up once, no file is to be skipped, and the worker stayed")
            {
                INFO (lines.joinIntoString ("\n"));
                REQUIRE (raised->load() == 1);
                REQUIRE (first);
                REQUIRE (second);
                REQUIRE (third);
                REQUIRE (lines.joinIntoString ("\n").contains ("TestA  asks 1  ->  NOT SCANNED: declined"));
                REQUIRE (scanner.findPluginTypesFor (format, found, "22"));
                REQUIRE (found.size() == 2);
                REQUIRE (scanner.getNumWorkersStarted() == 1);
            }

            AND_WHEN ("the next scan comes to such a file")
            {
                scanner.scanFinished();
                REQUIRE (scanner.findPluginTypesFor (format, found, "asks 1"));

                THEN ("the question is raised again")
                {
                    REQUIRE (raised->load() == 2);
                }
            }
        }
    }
}

SCENARIO ("the child-process scanner tells of the files a worker found no plugin in", "[engine][effects][hosting][scan]")
{
    TestPluginFormat format ("TestA", std::vector { gainIn ("TestA", 0x1111) });

    GIVEN ("workers that find a plugin in one file, none in another, and die of a third")
    {
        ChildProcessScanner scanner ([]
        {
            return std::make_unique<ThreadWorker> ([] (const ChildProcessScanner::Request& request) -> json
            {
                if (request.file == "empty")
                    return { { "ok", true }, { "plugins", json::array() } };
                return describedIn (request);
            }, patientIdleMs);
        }, patientTimeoutMs);
        juce::StringArray heard;
        scanner.setFoundNothing ([&heard] (const juce::String& file) { heard.add (file); });
        juce::OwnedArray<juce::PluginDescription> found;

        WHEN ("they are scanned")
        {
            REQUIRE (scanner.findPluginTypesFor (format, found, "11"));
            REQUIRE (scanner.findPluginTypesFor (format, found, "empty"));
            REQUIRE_FALSE (scanner.findPluginTypesFor (format, found, "dies"));

            THEN ("only the empty one is told of")
            {
                REQUIRE (heard == juce::StringArray { "empty" });
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

            THEN ("the pipe is given up after the first, with nothing to fall back to here - which is not held against the files")
            {
                REQUIRE (first);
                REQUIRE (second);
                REQUIRE (found.isEmpty());
                REQUIRE (scanner.getNumWorkersStarted() == 1);
            }
        }
    }
}

#if JUCE_MAC && defined (AUDIONAUT_TEST_SCAN_WORKER)
// Real Audio Units in a real worker process: the two components of Catch2Tests/TestAudioUnits and
// Apple's AUDelay, scanned by AudionautTestScanWorker - the scan helper's worker, which registers
// the test components in its own process instead of having them installed on the machine.
SCENARIO ("a scan gets past an Audio Unit that crashes and one with nothing in it", "[engine][effects][hosting][scan][audiounit]")
{
    MessageThread messageThread;

    const juce::String crashes ("AudioUnit:Effects/aufx,Cras,Audn");
    const juce::String empty ("AudioUnit:Effects/aufx,Empt,Audn");
    const juce::String crashesInEditor ("AudioUnit:Effects/aufx,CrEd,Audn");
    const juce::String delay ("AudioUnit:Effects/aufx,dely,appl");

    GIVEN ("a plugin list that scans in the test worker")
    {
        const juce::File workerExecutable (AUDIONAUT_TEST_SCAN_WORKER);
        REQUIRE (workerExecutable.existsAsFile());

        juce::AudioUnitPluginFormat format;
        juce::KnownPluginList list;
        juce::StringArray log;

        auto ownedScanner = std::make_unique<ChildProcessScanner> (workerExecutable, 30000);
        auto* scanner = ownedScanner.get();
        scanner->setLog ([&log] (const juce::String& line) { log.add (line); });
        list.setCustomScanner (std::move (ownedScanner));

        const auto scan = [&] (const juce::String& identifier)
        {
            juce::OwnedArray<juce::PluginDescription> found;
            list.scanAndAddFile (identifier, true, found, format);
            return found.size();
        };
        const auto logged = [&] (const juce::String& identifier, const juce::String& outcome)
        {
            for (const auto& line : log)
                if (line.contains (identifier + "  ->  ") && line.contains (outcome))
                    return true;
            return false;
        };

        WHEN ("the empty Audio Unit is scanned")
        {
            const auto found = scan (empty);

            THEN ("no plugin is found, and nothing is held against the file")
            {
                REQUIRE (found == 0);
                REQUIRE (list.getNumTypes() == 0);
                REQUIRE (list.getBlacklistedFiles().isEmpty());
                REQUIRE (logged (empty, "no plugin found"));
                REQUIRE (scanner->getNumWorkersStarted() == 1);
            }

            THEN ("the same worker goes on to the next file")
            {
                REQUIRE (scan (delay) == 1);
                REQUIRE (scanner->getNumWorkersStarted() == 1);
            }
        }

        WHEN ("the Audio Unit that only crashes with its editor open is scanned")
        {
            const auto found = scan (crashesInEditor);

            THEN ("it is listed like any effect: the scan opens no editor")
            {
                REQUIRE (found == 1);
                REQUIRE (list.getBlacklistedFiles().isEmpty());
                REQUIRE (list.getTypes()[0].name == "Crash On Editor");
                REQUIRE (list.getTypes()[0].manufacturerName == "Audionaut Tests");
                REQUIRE (list.getTypes()[0].numInputChannels == 2);
                REQUIRE (list.getTypes()[0].numOutputChannels == 2);
                REQUIRE_FALSE (list.getTypes()[0].isInstrument);
                REQUIRE (scanner->getNumWorkersStarted() == 1);
            }
        }

        WHEN ("the crashing Audio Unit is the first file of a scan")
        {
            const auto found = scan (crashes);

            THEN ("it is skipped from now on, and the next file is scanned by a new worker")
            {
                REQUIRE (found == 0);
                REQUIRE (list.getBlacklistedFiles() == juce::StringArray { crashes });
                REQUIRE (logged (crashes, "FAILED, file skipped: the worker process ended while the plugin was loading"));
                REQUIRE (scanner->getNumWorkersStarted() == 1);

                REQUIRE (scan (delay) == 1);
                REQUIRE (scanner->getNumWorkersStarted() == 2);
                REQUIRE (list.getNumTypes() == 1);
                REQUIRE (list.getTypes()[0].name == "AUDelay");
            }
        }

        WHEN ("it comes after a file the worker had loaded, and the empty one after it")
        {
            REQUIRE (scan (delay) == 1);
            const auto foundInCrashing = scan (crashes);
            const auto foundInEmpty = scan (empty);

            THEN ("it got a worker to itself before it was skipped, and only it is skipped")
            {
                REQUIRE (foundInCrashing == 0);
                REQUIRE (foundInEmpty == 0);
                REQUIRE (scanner->getNumWorkersStarted() == 3);   // the first, one for the crashing file alone, one for what follows
                REQUIRE (list.getBlacklistedFiles() == juce::StringArray { crashes });
                REQUIRE (list.getNumTypes() == 1);
            }
        }
    }
}
#endif
