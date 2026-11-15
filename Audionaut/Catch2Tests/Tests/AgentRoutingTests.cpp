#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <thread>

#include "Cli/AgentClient.h"
#include "Cli/AgentHost.h"
#include "Cli/AgentProtocol.h"
#include "Cli/CommandSession.h"
#include "Cli/Commands/Commands.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Effects/Hosting/HostedEffect.h"
#include "Engine/Effects/Hosting/PluginHost.h"
#include "Engine/Effects/InsertChain.h"
#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Engine/Project/ProjectSerializer.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "TestPluginInstance.h"

using namespace audium;
using namespace audium::cli;

namespace {

juce::File makeRoutingWorkDirectory()
{
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                   .getChildFile ("audionaut-routing-tests");
    dir.deleteRecursively();
    REQUIRE (dir.createDirectory());
    return dir;
}

/** The host marshals work to the message thread; a test process has no
    dispatch loop to marshal to, so run it where we are. */
agent::AgentHost::MessageThreadExecutor inlineExecutor()
{
    return [] (std::function<void()> task) { task(); };
}

juce::ArgumentList argsFor (const juce::String& commandLine)
{
    return juce::ArgumentList ("audionaut-cli", commandLine);
}

/** setenv is POSIX; MSVC has _putenv_s, where an empty value removes it. */
void setEnvironmentVariable (const char* name, const char* value)
{
#if JUCE_WINDOWS
    _putenv_s (name, value != nullptr ? value : "");
#else
    if (value == nullptr || *value == '\0')
        ::unsetenv (name);
    else
        ::setenv (name, value, 1);
#endif
}

/** Sets a variable for the scope and unsets it on the way out - also past a
    failed REQUIRE, so no later scenario inherits it. */
struct ScopedEnvironmentVariable
{
    ScopedEnvironmentVariable (const char* name_, const char* value) : name (name_)
    {
        setEnvironmentVariable (name, value);
    }

    ~ScopedEnvironmentVariable() { setEnvironmentVariable (name, nullptr); }

    const char* name;
};

} // namespace

// The point of routing: when the app holds a project, a verb must see what the
// user sees - not the last save - and must not write their file.
SCENARIO("a verb addressed at a held project is answered by its host",
         "[cli][agent][routing]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock (Thread::getCurrentThread());

    auto workDir = makeRoutingWorkDirectory();
    auto package = workDir.getChildFile ("routed.audium");
    auto projectJson = package.getChildFile (ProjectFileStore::projectFileName);

    // a "live" app: an engine holding a saved project
    auto engine = AudiumFactory::createAudiumEngine();
    engine->getProjectSerializer()->createNewProject();
    engine->getAudioTrackContainer()->setMasterGain (1.0f);
    REQUIRE (engine->getProjectFileStore()->save (projectJson, nullptr));

    CliContext context;
    context.quiet = true;
    context.json = true;

    GIVEN("no host for the project") {
        THEN("the verb is left to run against the file") {
            auto outcome = agent::routeCommand (argsFor ("info " + package.getFullPathName()),
                                                "info", true, context);
            REQUIRE_FALSE (outcome.handled);
            REQUIRE_FALSE (agent::markerFileFor (projectJson).existsAsFile());
        }
    }

    GIVEN("the app hosting that project") {
        agent::AgentHost host (engine, inlineExecutor());
        host.setProject (projectJson);

        REQUIRE (host.isHosting());
        REQUIRE (agent::markerFileFor (projectJson).existsAsFile());

        const auto marker = agent::readHostMarker (projectJson);
        REQUIRE (marker.has_value());
        REQUIRE (marker->port == host.boundPort());

        WHEN("the document has unsaved changes the file knows nothing about") {
            engine->getAudioTrackContainer()->setMasterGain (0.3f);

            const auto savedAt = projectJson.getLastModificationTime();

            json envelope;
            context.envelopeSink = [&envelope] (const json& produced) { envelope = produced; };

            auto outcome = agent::routeCommand (argsFor ("info " + package.getFullPathName() + " --raw"),
                                                "info", true, context);

            THEN("the host answers from the live graph, and the file is untouched") {
                REQUIRE (outcome.handled);
                REQUIRE (outcome.exitCode == exitOk);
                REQUIRE (envelope.value ("ok", false));

                const auto result = envelope.value ("result", json::object());
                REQUIRE (result.contains ("audium"));
                REQUIRE (result["audium"]["master_gain"].get<double>() == Catch::Approx (0.3));

                REQUIRE (projectJson.getLastModificationTime() == savedAt);
            }
        }

        WHEN("routing is switched off for the process") {
            THEN("the verb goes back to the file even though a host is there") {
                // the kill switch has to work without stopping the host
                setEnvironmentVariable (agent::routingDisabledEnvVar, "0");
                auto outcome = agent::routeCommand (argsFor ("info " + package.getFullPathName()),
                                                    "info", true, context);
                setEnvironmentVariable (agent::routingDisabledEnvVar, nullptr);

                REQUIRE_FALSE (outcome.handled);
            }
        }

        WHEN("the verb is one the host does not serve") {
            THEN("it is left to the file rather than refused") {
                // `create` has no open document to run against
                auto outcome = agent::routeCommand (argsFor ("create " + package.getFullPathName()),
                                                    "create", agent::isHostableVerb ("create"),
                                                    context);
                REQUIRE_FALSE (outcome.handled);
            }
        }

        WHEN("the host goes away without withdrawing its marker") {
            // stands in for a crash: the marker outlives the process
            const auto stolen = agent::markerFileFor (projectJson).loadFileAsString();
            host.setProject (juce::File());
            REQUIRE_FALSE (agent::markerFileFor (projectJson).existsAsFile());
            agent::markerFileFor (projectJson).replaceWithText (stolen);

            THEN("a marker naming a dead process is ignored") {
                auto dead = json::parse (stolen.toStdString());
                dead["pid"] = 999999; // no such process
                agent::markerFileFor (projectJson).replaceWithText (dead.dump());

                REQUIRE_FALSE (agent::readHostMarker (projectJson).has_value());

                auto outcome = agent::routeCommand (argsFor ("info " + package.getFullPathName()),
                                                    "info", true, context);
                REQUIRE_FALSE (outcome.handled);
            }

            AND_THEN("a marker naming a live process that does not answer is refused") {
                // our own pid is alive, but nothing is listening on this port
                auto unanswered = json::parse (stolen.toStdString());
                unanswered["port"] = 1; // reserved, nothing of ours is there
                agent::markerFileFor (projectJson).replaceWithText (unanswered.dump());

                json envelope;
                context.envelopeSink = [&envelope] (const json& produced) { envelope = produced; };

                auto outcome = agent::routeCommand (argsFor ("info " + package.getFullPathName()),
                                                    "info", true, context);

                REQUIRE (outcome.handled); // refused, NOT sent to the file
                REQUIRE (outcome.exitCode == exitFailure);
                REQUIRE_FALSE (envelope.value ("ok", true));
                REQUIRE (envelope["error"]["code"] == "host_unavailable");
            }

            // these sections plant markers by hand; do not leave them for the
            // assertions below
            agent::markerFileFor (projectJson).deleteFile();
        }

        host.setProject (juce::File());
        REQUIRE_FALSE (agent::markerFileFor (projectJson).existsAsFile());
    }

    engine = nullptr;
    workDir.deleteRecursively();
    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

// A verb that changes the project is the case the whole design exists for: it
// must reach the open document and leave the file alone.
SCENARIO("a hosted verb changes the open document, not the project file",
         "[cli][agent][routing][mutation]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock (Thread::getCurrentThread());

    auto workDir = makeRoutingWorkDirectory();
    auto package = workDir.getChildFile ("mutated.audium");
    auto projectJson = package.getChildFile (ProjectFileStore::projectFileName);

    CliContext setup;
    setup.quiet = true;

    // a project with one clip, built the ordinary way
    REQUIRE (runCreate (argsFor ("create " + package.getFullPathName()), setup) == exitOk);
    const auto sourceAudio = juce::String (CURRENT_SOURCE_DIR) + "/TestFiles/120-funk-1-sec.wav";
    REQUIRE (runImport (argsFor ("import " + package.getFullPathName() + " " + sourceAudio), setup) == exitOk);

    // the "app" holding it
    auto engine = AudiumFactory::createAudiumEngine();
    REQUIRE (engine->getProjectFileStore()->open (projectJson, nullptr));

    CliContext context;
    context.quiet = true;
    context.json = true;

    json envelope;
    context.envelopeSink = [&envelope] (const json& produced) { envelope = produced; };

    GIVEN("the app hosting it, with the file as last saved") {
        agent::AgentHost host (engine, inlineExecutor());
        host.setProject (projectJson);
        REQUIRE (host.isHosting());

        const auto savedAt = projectJson.getLastModificationTime();
        REQUIRE_FALSE (engine->getUndoManager()->canUndo());

        WHEN("an agent sets a clip gain") {
            auto outcome = agent::routeCommand (
                argsFor ("clip-gain " + package.getFullPathName() + " --at 1 --gain 0.5"),
                "clip-gain", agent::isHostableVerb ("clip-gain"), context);

            THEN("it lands on the open document as one undoable step, and the file is untouched") {
                REQUIRE (outcome.handled);
                REQUIRE (envelope.value ("ok", false));
                REQUIRE (outcome.exitCode == exitOk);

                REQUIRE (projectJson.getLastModificationTime() == savedAt);
                REQUIRE (engine->getProjectFileStore()->wasChangedExternally());
                REQUIRE (engine->getUndoManager()->canUndo());

                json afterState;
                engine->getProjectSerializer()->writeToJson (afterState);

                REQUIRE (engine->getUndoManager()->undo());

                json undoneState;
                engine->getProjectSerializer()->writeToJson (undoneState);
                REQUIRE (undoneState != afterState);
                REQUIRE_FALSE (engine->getProjectFileStore()->wasChangedExternally());
            }
        }

        host.setProject (juce::File());
    }

    GIVEN("the app hosting it while the transport is playing") {
        // The host snapshots the document on its first trip to the message
        // thread and applies on its second; move the playhead in between, as
        // playback does.
        auto scheduler = engine->getPlayListScheduler();
        int trips = 0;
        agent::AgentHost host (engine, [&] (std::function<void()> task) {
            if (++trips == 2)
                scheduler->data.transportPositionClocks = 480.0;
            task();
        });
        host.setProject (projectJson);
        REQUIRE (host.isHosting());

        WHEN("an agent sets a clip gain") {
            auto outcome = agent::routeCommand (
                argsFor ("clip-gain " + package.getFullPathName() + " --at 1 --gain 0.5"),
                "clip-gain", agent::isHostableVerb ("clip-gain"), context);

            THEN("the edit applies, and neither it nor its undo moves the playhead") {
                REQUIRE (outcome.handled);
                REQUIRE (envelope.value ("ok", false));
                REQUIRE (outcome.exitCode == exitOk);
                REQUIRE (scheduler->data.transportPositionClocks == 480.0);

                REQUIRE (engine->getUndoManager()->undo());
                REQUIRE (scheduler->data.transportPositionClocks == 480.0);
            }
        }

        host.setProject (juce::File());
    }

    engine = nullptr;
    workDir.deleteRecursively();
    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("the hosted verbs of one agent task become one undo step",
         "[cli][agent][routing][undo]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock (Thread::getCurrentThread());

    auto workDir = makeRoutingWorkDirectory();
    auto package = workDir.getChildFile ("task.audium");
    auto projectJson = package.getChildFile (ProjectFileStore::projectFileName);

    CliContext setup;
    setup.quiet = true;
    REQUIRE (runCreate (argsFor ("create " + package.getFullPathName()), setup) == exitOk);
    const auto sourceAudio = juce::String (CURRENT_SOURCE_DIR) + "/TestFiles/120-funk-1-sec.wav";
    REQUIRE (runImport (argsFor ("import " + package.getFullPathName() + " " + sourceAudio), setup) == exitOk);

    auto engine = AudiumFactory::createAudiumEngine();
    REQUIRE (engine->getProjectFileStore()->open (projectJson, nullptr));
    auto undoManager = engine->getUndoManager();

    CliContext context;
    context.quiet = true;
    context.json = true;
    json envelope;
    context.envelopeSink = [&envelope] (const json& produced) { envelope = produced; };

    auto runHosted = [&] (const juce::String& verb, const juce::String& options) {
        auto outcome = agent::routeCommand (argsFor (verb + " " + package.getFullPathName() + " " + options),
                                            verb, agent::isHostableVerb (verb), context);
        return outcome.handled && outcome.exitCode == exitOk && envelope.value ("ok", false);
    };

    auto numClips = [&] {
        int count = 0;
        for (auto& track : engine->getAudioTrackContainer()->getAudioTracks())
            count += track->getPlayListContainer()->getNumItems();
        return count;
    };

    json original;
    engine->getProjectSerializer()->writeToJson (original);
    REQUIRE (numClips() == 1);

    GIVEN("the app hosting it and an MCP session working on a named task") {
        agent::AgentHost host (engine, inlineExecutor());
        host.setProject (projectJson);
        REQUIRE (host.isHosting());

        // what the MCP server sets for every CLI it spawns
        const ScopedEnvironmentVariable session ("AUDIONAUT_AGENT_SESSION", "session-1");
        const ScopedEnvironmentVariable task ("AUDIONAUT_AGENT_TASK", "trim the intro");

        WHEN("it splits a clip and then changes a gain") {
            REQUIRE (runHosted ("split", "--at 0.5 --unit seconds"));
            REQUIRE (runHosted ("clip-gain", "--at 0.25 --unit seconds --gain 0.5"));

            THEN("the document shows both, as one step named after the task") {
                REQUIRE (undoManager->getUndoDescriptions().size() == 1);
                REQUIRE (undoManager->getUndoDescription() == "Agent: trim the intro");
                REQUIRE (numClips() == 2);

                AND_THEN("one undo takes the whole task back") {
                    REQUIRE (undoManager->undo());
                    json undone;
                    engine->getProjectSerializer()->writeToJson (undone);
                    REQUIRE (ProjectFileStore::documentOf (undone) == ProjectFileStore::documentOf (original));
                }
            }

            AND_WHEN("a bare CLI call with no session follows") {
                setEnvironmentVariable ("AUDIONAUT_AGENT_SESSION", nullptr);
                setEnvironmentVariable ("AUDIONAUT_AGENT_TASK", nullptr);
                REQUIRE (runHosted ("clip-gain", "--at 0.75 --unit seconds --gain 0.5"));

                THEN("it is a step of its own") {
                    REQUIRE (undoManager->getUndoDescriptions().size() == 2);
                    REQUIRE (undoManager->getUndoDescription() == "Agent edits");
                }
            }
        }

        host.setProject (juce::File());
    }

    engine = nullptr;
    workDir.deleteRecursively();
    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

// A hosted edit works on a copy of the document that lists the plugins without
// loading them. Two verbs need the plugins themselves: one that sets a plugin's
// parameter (the parameters are the plugin's), and an export (without them the
// file would be the project minus its plugins).
SCENARIO("a hosted verb reaches the plugins of the open document",
         "[cli][agent][routing][hosting]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock (Thread::getCurrentThread());

    auto workDir = makeRoutingWorkDirectory();
    auto package = workDir.getChildFile ("plugins.audium");
    auto projectJson = package.getChildFile (ProjectFileStore::projectFileName);

    CliContext setup;
    setup.quiet = true;
    REQUIRE (runCreate (argsFor ("create " + package.getFullPathName()), setup) == exitOk);
    const auto sourceAudio = juce::String (CURRENT_SOURCE_DIR) + "/TestFiles/120-funk-1-sec.wav";
    REQUIRE (runImport (argsFor ("import " + package.getFullPathName() + " " + sourceAudio), setup) == exitOk);

    // the machine's plugins: one test format with one gain plugin
    TestPluginInstance::Options plugin;
    plugin.format = "TestA";
    plugin.uid = 0x1111;
    auto pluginHost = std::make_shared<PluginHost> (workDir.getChildFile ("data"), false);
    pluginHost->getFormats().addFormat (std::make_unique<TestPluginFormat> ("TestA", std::vector { plugin }));
    pluginHost->getList().addType (TestPluginInstance::describe (plugin));

    auto engine = AudiumFactory::createAudiumEngine (nullptr, pluginHost);
    REQUIRE (engine->getProjectFileStore()->open (projectJson, nullptr));

    // the plugin on the track that holds the clip
    std::shared_ptr<AudioTrack> track;
    for (auto& candidate : engine->getAudioTrackContainer()->getAudioTracks())
        if (candidate->getPlayListContainer()->getNumItems() > 0)
            track = candidate;
    REQUIRE (track != nullptr);
    auto slot = track->getTrackInserts().add ("testa:00001111");
    REQUIRE (slot != nullptr);
    auto* live = dynamic_cast<HostedEffect*> (&slot->getEffect());
    REQUIRE (live != nullptr);
    REQUIRE (live->isLoaded());
    const auto trackOption = "--track " + juce::String (track->getId());
    const auto instancesWhileOpen = TestPluginInstance::instances();

    CliContext context;
    context.quiet = true;
    context.json = true;
    json envelope;
    context.envelopeSink = [&envelope] (const json& produced) { envelope = produced; };

    // As in the app: the host runs the verb on the message thread - where a plugin is created - while
    // the client waits on a thread of its own, so this thread serves the message loop meanwhile.
    auto runHosted = [&] (const juce::String& verb, const juce::String& options) {
        std::atomic<bool> done { false };
        auto ok = false;
        std::thread client ([&] {
            auto outcome = agent::routeCommand (argsFor (verb + " " + package.getFullPathName() + " " + options),
                                                verb, agent::isHostableVerb (verb), context);
            ok = outcome.handled && outcome.exitCode == exitOk && envelope.value ("ok", false);
            done = true;
        });
        while (! done)
            MessageManager::getInstance()->runDispatchLoopUntil (10);
        client.join();
        return ok;
    };

    GIVEN("the app hosting the document") {
        agent::AgentHost host (engine);   // the app's own executor: onto the message thread
        host.setProject (projectJson);
        REQUIRE (host.isHosting());

        WHEN("an agent sets a parameter of the plugin") {
            const auto set = runHosted ("set-insert", trackOption + " --index 0 --param gain --value 0.25");

            THEN("the plugin of the open document has the value, and the copy's plugin is gone again") {
                INFO (envelope.dump());
                REQUIRE (set);
                REQUIRE (live->getParameter (live->getParameterIndex ("gain")) == Catch::Approx (0.25f));
                REQUIRE (engine->getUndoManager()->canUndo());
                REQUIRE (TestPluginInstance::instances() == instancesWhileOpen);
            }
        }

        WHEN("the plugin mutes the track and an agent exports the project") {
            live->setParameter (live->getParameterIndex ("mute"), 1.0f);
            const auto output = workDir.getChildFile ("muted.wav");
            const auto exported = runHosted ("export", "-o " + output.getFullPathName());

            THEN("the file is what the document sounds like: the plugin ran") {
                INFO (envelope.dump());
                REQUIRE (exported);

                juce::AudioFormatManager formatManager;
                formatManager.registerBasicFormats();
                std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (output));
                REQUIRE (reader != nullptr);
                juce::AudioBuffer<float> buffer (static_cast<int> (reader->numChannels), static_cast<int> (reader->lengthInSamples));
                reader->read (&buffer, 0, buffer.getNumSamples(), 0, true, true);
                REQUIRE (buffer.getNumSamples() > 0);
                REQUIRE (buffer.getMagnitude (0, buffer.getNumSamples()) == Catch::Approx (0.0).margin (1.0e-4));
                REQUIRE (TestPluginInstance::instances() == instancesWhileOpen);
            }
        }

        host.setProject (juce::File());
    }

    slot = nullptr;
    track = nullptr;
    engine = nullptr;
    pluginHost = nullptr;
    workDir.deleteRecursively();
    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

// The app's export and stem separation render on a worker thread behind a
// modal progress window whose loop still serves agents. A verb applied then
// would rebuild the graph the render is walking, so it waits its turn.
SCENARIO("a hosted verb waits for the app's render to finish",
         "[cli][agent][routing][render]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock (Thread::getCurrentThread());

    auto workDir = makeRoutingWorkDirectory();
    auto package = workDir.getChildFile ("rendering.audium");
    auto projectJson = package.getChildFile (ProjectFileStore::projectFileName);

    CliContext setup;
    setup.quiet = true;

    REQUIRE (runCreate (argsFor ("create " + package.getFullPathName()), setup) == exitOk);
    const auto sourceAudio = juce::String (CURRENT_SOURCE_DIR) + "/TestFiles/120-funk-1-sec.wav";
    REQUIRE (runImport (argsFor ("import " + package.getFullPathName() + " " + sourceAudio), setup) == exitOk);

    auto engine = AudiumFactory::createAudiumEngine();
    REQUIRE (engine->getProjectFileStore()->open (projectJson, nullptr));
    auto scheduler = engine->getPlayListScheduler();

    CliContext context;
    context.quiet = true;
    context.json = true;

    json envelope;
    context.envelopeSink = [&envelope] (const json& produced) { envelope = produced; };

    const auto clipGain = argsFor ("clip-gain " + package.getFullPathName() + " --at 1 --gain 0.5");
    const auto savedAt = projectJson.getLastModificationTime();

    GIVEN("the app hosting it while an export renders") {
        agent::AgentHost host (engine, inlineExecutor());
        host.setProject (projectJson);
        REQUIRE (host.isHosting());

        // what the app holds from before the export's worker starts until it
        // has finished
        auto render = std::make_unique<PlayListScheduler::ScopedOfflineRender> (*scheduler);

        WHEN("an agent sets a clip gain") {
            auto outcome = agent::routeCommand (clipGain, "clip-gain", agent::isHostableVerb ("clip-gain"), context);

            THEN("it is refused, and neither the document nor the file changed") {
                REQUIRE (outcome.handled); // refused, NOT sent to the file
                REQUIRE (outcome.exitCode == exitFailure);
                REQUIRE_FALSE (envelope.value ("ok", true));
                REQUIRE (envelope["error"]["code"] == "render_in_progress");
                REQUIRE_FALSE (engine->getUndoManager()->canUndo());
                REQUIRE (projectJson.getLastModificationTime() == savedAt);

                AND_THEN("the same verb goes through once the render is over") {
                    render.reset();
                    envelope = json();

                    auto retry = agent::routeCommand (clipGain, "clip-gain", agent::isHostableVerb ("clip-gain"), context);
                    REQUIRE (retry.handled);
                    REQUIRE (retry.exitCode == exitOk);
                    REQUIRE (envelope.value ("ok", false));
                    REQUIRE (engine->getUndoManager()->canUndo());
                    REQUIRE (projectJson.getLastModificationTime() == savedAt);
                }
            }
        }

        render.reset();
        host.setProject (juce::File());
    }

    GIVEN("the app hosting it, with an export started while the verb was in flight") {
        // The host looks on its first trip to the message thread and applies
        // on its second; the thread is free in between, so the user can start
        // an export there.
        std::unique_ptr<PlayListScheduler::ScopedOfflineRender> render;
        int trips = 0;
        agent::AgentHost host (engine, [&] (std::function<void()> task) {
            if (++trips == 2)
                render = std::make_unique<PlayListScheduler::ScopedOfflineRender> (*scheduler);
            task();
        });
        host.setProject (projectJson);
        REQUIRE (host.isHosting());

        WHEN("an agent sets a clip gain") {
            auto outcome = agent::routeCommand (clipGain, "clip-gain", agent::isHostableVerb ("clip-gain"), context);

            THEN("the second look refuses it, and nothing was applied") {
                REQUIRE (trips == 2);
                REQUIRE (outcome.handled);
                REQUIRE (outcome.exitCode == exitFailure);
                REQUIRE (envelope["error"]["code"] == "render_in_progress");
                REQUIRE_FALSE (engine->getUndoManager()->canUndo());
            }
        }

        render.reset();
        host.setProject (juce::File());
    }

    engine = nullptr;
    workDir.deleteRecursively();
    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

// A separation runs for minutes. An agent waiting on it must hear progress as
// it happens, not one batch at the end - and an older client that cannot read
// streamed messages must still get the log it always got.
SCENARIO("a hosted verb streams its progress to the client",
         "[cli][agent][routing][progress]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock (Thread::getCurrentThread());

    auto workDir = makeRoutingWorkDirectory();
    auto package = workDir.getChildFile ("progress.audium");
    auto projectJson = package.getChildFile (ProjectFileStore::projectFileName);

    CliContext setup;
    setup.quiet = true;

    REQUIRE (runCreate (argsFor ("create " + package.getFullPathName() + " --channels 1"), setup) == exitOk);
    const auto sourceAudio = juce::String (CURRENT_SOURCE_DIR) + "/TestFiles/sine-0dB.wav";
    REQUIRE (runImport (argsFor ("import " + package.getFullPathName() + " " + sourceAudio), setup) == exitOk);

    auto engine = AudiumFactory::createAudiumEngine();
    REQUIRE (engine->getProjectFileStore()->open (projectJson, nullptr));

    agent::AgentHost host (engine, inlineExecutor());
    host.setProject (projectJson);
    REQUIRE (host.isHosting());

    const auto separate = "separate " + package.getFullPathName() + " --track 1 --backend fake --threads 1";

    GIVEN("a client that streams") {
        CliContext context;
        context.json = true;

        json envelope;
        std::vector<double> fractions;
        juce::StringArray logLines;
        context.envelopeSink = [&envelope] (const json& produced) { envelope = produced; };
        context.progressSink = [&fractions] (double fraction, const juce::String&) { fractions.push_back (fraction); };
        context.logSink      = [&logLines] (const juce::String& line) { logLines.add (line); };

        WHEN("an agent separates a clip") {
            auto outcome = agent::routeCommand (argsFor (separate), "separate",
                                                agent::isHostableVerb ("separate"), context);

            THEN("each step arrives as progress, in order, before the result") {
                REQUIRE (outcome.handled);
                REQUIRE (outcome.exitCode == exitOk);
                REQUIRE (envelope.value ("ok", false));

                REQUIRE (fractions.size() > 2);
                REQUIRE (fractions.front() == Catch::Approx (0.0));
                REQUIRE (fractions.back() == Catch::Approx (1.0));
                REQUIRE (std::is_sorted (fractions.begin(), fractions.end()));

                // ...and none of it is folded into the log as text
                for (auto& line : logLines)
                    REQUIRE_FALSE (line.endsWith ("%"));
                REQUIRE (logLines.contains ("stems added"));
            }
        }
    }

    GIVEN("a client that predates streaming") {
        // Speak the protocol by hand, as an old binary would: no "stream".
        struct OldClient final : juce::InterprocessConnection
        {
            OldClient() : juce::InterprocessConnection (false) {}
            ~OldClient() override { disconnect(); }
            void connectionMade() override {}
            void connectionLost() override { done.signal(); }
            void messageReceived (const juce::MemoryBlock& block) override
            {
                json message;
                if (! agent::decode (block, message))
                    return;
                kinds.add (message.value ("kind", std::string()));
                if (kinds.getReference (kinds.size() - 1) == "result") {
                    result = message;
                    done.signal();
                }
            }
            juce::StringArray kinds;
            json result;
            juce::WaitableEvent done;
        };

        OldClient client;
        REQUIRE (client.connectToSocket ("127.0.0.1", host.boundPort(), 2000));

        juce::StringArray argv;
        for (auto& argument : argsFor (separate).arguments)
            argv.add (argument.text);

        auto command = agent::makeCommand (projectJson, juce::File::getCurrentWorkingDirectory(), argv);
        command.erase ("stream");
        REQUIRE (client.sendMessage (agent::toBlock (command)));
        REQUIRE (client.done.wait (30000));

        THEN("only hello and result come back, with progress as log lines") {
            REQUIRE (client.kinds == juce::StringArray ({ "hello", "result" }));
            REQUIRE (client.result.value ("exitCode", exitFailure) == exitOk);

            juce::StringArray log;
            for (auto& line : client.result.value ("log", json::array()))
                log.add (juce::String (line.get<std::string>()));

            REQUIRE (log.contains ("Rendering clip 0%"));
            REQUIRE (log.contains ("stems added"));
        }
    }

    host.setProject (juce::File());
    engine = nullptr;
    workDir.deleteRecursively();
    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}

SCENARIO("a hosted verb that waits for a plugin does not run over what the user started meanwhile",
         "[cli][agent][routing][hosting]")
{
    MessageManager::getInstance();
    MessageManagerLock mmLock (Thread::getCurrentThread());

    auto workDir = makeRoutingWorkDirectory();
    auto package = workDir.getChildFile ("plugins.audium");
    auto projectJson = package.getChildFile (ProjectFileStore::projectFileName);

    CliContext setup;
    setup.quiet = true;
    REQUIRE (runCreate (argsFor ("create " + package.getFullPathName()), setup) == exitOk);
    const auto sourceAudio = juce::String (CURRENT_SOURCE_DIR) + "/TestFiles/120-funk-1-sec.wav";
    REQUIRE (runImport (argsFor ("import " + package.getFullPathName() + " " + sourceAudio), setup) == exitOk);

    // the setup verbs took their message loop with them: the plugin arrives through one
    MessageManager::getInstance();

    // the machine's plugins: one test format with one gain plugin
    TestPluginInstance::Options plugin;
    plugin.format = "TestA";
    plugin.uid = 0x1111;
    auto pluginHost = std::make_shared<PluginHost> (workDir.getChildFile ("data"), false);
    auto owned = std::make_unique<TestPluginFormat> ("TestA", std::vector { plugin });
    owned->asyncOnly = true;   // as a plugin that runs in a process of its own: it arrives through the message queue
    auto* format = owned.get();
    pluginHost->getFormats().addFormat (std::move (owned));
    pluginHost->getList().addType (TestPluginInstance::describe (plugin));

    auto engine = AudiumFactory::createAudiumEngine (nullptr, pluginHost);
    REQUIRE (engine->getProjectFileStore()->open (projectJson, nullptr));

    // the plugin on the track that holds the clip
    std::shared_ptr<AudioTrack> track;
    for (auto& candidate : engine->getAudioTrackContainer()->getAudioTracks())
        if (candidate->getPlayListContainer()->getNumItems() > 0)
            track = candidate;
    REQUIRE (track != nullptr);
    auto slot = track->getTrackInserts().add ("testa:00001111");
    REQUIRE (slot != nullptr);
    auto* live = dynamic_cast<HostedEffect*> (&slot->getEffect());
    REQUIRE (live != nullptr);
    REQUIRE (pluginHost->waitForPendingLoads (5000));
    REQUIRE (live->isLoaded());
    const auto trackOption = "--track " + juce::String (track->getId());
    const auto instancesWhileOpen = TestPluginInstance::instances();

    CliContext context;
    context.quiet = true;
    context.json = true;
    json envelope;
    context.envelopeSink = [&envelope] (const json& produced) { envelope = produced; };

    // As in the app: the host runs the verb on the message thread - where a plugin is created - while
    // the client waits on a thread of its own, so this thread serves the message loop meanwhile.
    auto runHosted = [&] (const juce::String& verb, const juce::String& options) {
        std::atomic<bool> done { false };
        auto ok = false;
        std::thread client ([&] {
            auto outcome = agent::routeCommand (argsFor (verb + " " + package.getFullPathName() + " " + options),
                                                verb, agent::isHostableVerb (verb), context);
            ok = outcome.handled && outcome.exitCode == exitOk && envelope.value ("ok", false);
            done = true;
        });
        while (! done)
            MessageManager::getInstance()->runDispatchLoopUntil (10);
        client.join();
        return ok;
    };

    GIVEN("the app hosting the document") {
        agent::AgentHost host (engine);   // the app's own executor: onto the message thread
        host.setProject (projectJson);
        REQUIRE (host.isHosting());

        WHEN("an export starts in the app while a set-insert waits for its plugin") {
            // the verb's copy of the document gets the plugin when its parameter is set, and the
            // message thread serves while it is on its way: that is when the user starts the export
            std::unique_ptr<PlayListScheduler::ScopedOfflineRender> render;
            format->onCreate = [&] { render = std::make_unique<PlayListScheduler::ScopedOfflineRender> (*engine->getPlayListScheduler()); };
            const auto before = live->getParameter (live->getParameterIndex ("gain"));
            const auto set = runHosted ("set-insert", trackOption + " --index 0 --param gain --value 0.25");
            format->onCreate = nullptr;

            THEN("the verb is refused when it comes to apply, and the document is as it was") {
                INFO (envelope.dump());
                REQUIRE (render != nullptr);
                REQUIRE_FALSE (set);
                REQUIRE (envelope["error"]["code"] == "render_in_progress");
                REQUIRE (live->getParameter (live->getParameterIndex ("gain")) == Catch::Approx (before));
                REQUIRE_FALSE (engine->getUndoManager()->canUndo());

                AND_THEN("it goes through once the export is over") {
                    render.reset();
                    REQUIRE (runHosted ("set-insert", trackOption + " --index 0 --param gain --value 0.25"));
                    REQUIRE (live->getParameter (live->getParameterIndex ("gain")) == Catch::Approx (0.25f));
                }
            }
            render.reset();
        }

        WHEN("a plugin of the open document is still on its way and an agent edits something else") {
            format->withhold = true;
            auto second = track->getTrackInserts().add ("testa:00001111");
            REQUIRE (second != nullptr);
            REQUIRE (pluginHost->getNumPendingLoads() == 1);

            const auto started = juce::Time::getMillisecondCounter();
            const auto edited = runHosted ("clip-gain", "--at 1 --gain 0.5");
            const auto took = juce::Time::getMillisecondCounter() - started;

            THEN("the verb does not wait for the app's plugin") {
                INFO (envelope.dump());
                REQUIRE (edited);
                REQUIRE (took < 10000);
                REQUIRE (pluginHost->getNumPendingLoads() == 1);
            }
            second = nullptr;
        }

        host.setProject (juce::File());
    }

    slot = nullptr;
    track = nullptr;
    engine = nullptr;
    pluginHost = nullptr;
    workDir.deleteRecursively();
    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}
