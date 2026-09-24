#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include <cstdlib>

#include "Cli/AgentClient.h"
#include "Cli/AgentHost.h"
#include "Cli/AgentProtocol.h"
#include "Cli/CommandSession.h"
#include "Cli/Commands/Commands.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Engine/Project/ProjectSerializer.h"
#include "Engine/Group/AudioTrackContainer.h"

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
                ::setenv (agent::routingDisabledEnvVar, "0", 1);
                auto outcome = agent::routeCommand (argsFor ("info " + package.getFullPathName()),
                                                    "info", true, context);
                ::unsetenv (agent::routingDisabledEnvVar);

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

    engine = nullptr;
    workDir.deleteRecursively();
    DeletedAtShutdown::deleteAll();
    MessageManager::deleteInstance();
}
