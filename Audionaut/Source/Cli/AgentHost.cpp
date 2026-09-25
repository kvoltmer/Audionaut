//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Cli/AgentHost.h"
#include "Cli/AgentClient.h"
#include "Cli/AgentProtocol.h"
#include "Cli/CliContext.h"
#include "Cli/CliDispatch.h"
#include "Cli/CommandSession.h"
#include "Cli/Commands/Commands.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Engine/Project/ProjectSerializer.h"
#include "Engine/Playback/AudioBusInterface.h"
#include "Engine/PlayList/PlayListScheduler.h"

#if JUCE_WINDOWS
 #include <windows.h>
#else
 #include <unistd.h>
#endif

namespace audium {
namespace cli {
namespace agent {

namespace {

int hostProcessId()
{
#if JUCE_WINDOWS
    return (int) GetCurrentProcessId();
#else
    return (int) getpid();
#endif
}

} // namespace

// ============================================================================

/**
 * One client. Greets on connect so the client can check it reached the right
 * app and project, then runs whatever verb arrives and replies.
 */
class AgentHost::Connection final : public juce::InterprocessConnection
{
public:
    Connection (AgentHost& host_) :
        // The work must land on the message thread, but taking callbacks there
        // would deadlock the executor we use to get there; take them on the
        // connection thread and marshal explicitly.
        juce::InterprocessConnection (false),
        host (host_)
    {
    }

    // JUCE requires this before the base destructor, or a pending callback
    // could land on a half-destroyed connection.
    ~Connection() override { disconnect(); }

    void connectionMade() override
    {
        sendMessage (toBlock (makeHello (host.projectFile, hostProcessId())));
    }

    void connectionLost() override {}

    void messageReceived (const juce::MemoryBlock& block) override
    {
        json request;
        if (! decode (block, request))
            return;

        if (request.value ("kind", std::string()) != "command")
            return;

        if (request.value ("protocol", 0) > protocolVersion) {
            reply (exitFailure, errorEnvelope ("protocol_mismatch",
                                               "this Audionaut is older than the client; update the app"), {});
            return;
        }

        juce::StringArray argv;
        for (auto& argument : request.value ("argv", json::array()))
            if (argument.is_string())
                argv.add (juce::String (argument.get<std::string>()));

        if (argv.isEmpty()) {
            reply (exitUsage, errorEnvelope ("usage", "no command given"), {});
            return;
        }

        if (! isHostableVerb (argv[0])) {
            // The client only routes verbs it believes are served; reaching
            // here means the two disagree, so say so rather than guess.
            reply (exitFailure, errorEnvelope ("verb_not_hosted",
                                               ("\"" + argv[0] + "\" cannot run against the open document").toStdString()),
                   {});
            return;
        }

        const juce::File workingDirectory (juce::String (request.value ("cwd", std::string())));

        // One command at a time: a separation runs for minutes, and two verbs
        // interleaving on one document would each undo the other's work.
        if (host.busy.exchange (true)) {
            reply (exitFailure, errorEnvelope ("host_busy",
                                               "Audionaut is already running a command for an agent"), {});
            return;
        }

        run (argv, workingDirectory);
        host.busy = false;
    }

private:
    static json errorEnvelope (const std::string& code, const std::string& message)
    {
        return { { "ok", false }, { "error", { { "code", code }, { "message", message } } } };
    }

    void reply (int exitCode, const json& envelope, const juce::StringArray& log)
    {
        sendMessage (toBlock (makeResult (exitCode, envelope, log)));
    }

    void run (const juce::StringArray& argv, const juce::File& workingDirectory)
    {
        json envelope;
        juce::StringArray log;
        auto exitCode = exitFailure;

        // Everything that touches the graph happens on the message thread, so
        // it never runs beside the user's own edits.
        json liveState;
        Refusal refusal;

        host.messageThreadExecutor ([&] {
            refusal = host.refusalFor (argv[0]);
            if (refusal.message.empty())
                host.engine->getProjectSerializer()->writeToJson (liveState);
        });

        if (! refusal.message.empty()) {
            reply (exitFailure, errorEnvelope (refusal.code, refusal.message), {});
            return;
        }

        host.messageThreadExecutor ([&] {
            // The message thread was free between the two trips, so the user
            // may have started an export or a take since we looked.
            refusal = host.refusalFor (argv[0]);
            if (! refusal.message.empty())
                return;

            CliContext context;
            // A host always wants the envelope, never the human rendering:
            // the client re-renders it for whoever actually asked.
            context.json = true;
            context.envelopeSink = [&envelope] (const json& produced) { envelope = produced; };
            context.logSink      = [&log] (const juce::String& line)  { log.add (line); };

            const ScopedWorkingDirectory workingDirectoryScope (workingDirectory);
            const HostedSessionScope hostedScope (host.engine, host.projectFile, liveState);

            juce::ArgumentList arguments ("audionaut", argv);
            exitCode = performCliCommand (arguments, context);

            if (exitCode == exitOk && HostedSessionScope::hasStagedState())
                applyStaged (argv[0], liveState, HostedSessionScope::takeStagedState(),
                             exitCode, envelope);
        });

        if (! refusal.message.empty()) {
            reply (exitFailure, errorEnvelope (refusal.code, refusal.message), {});
            return;
        }

        if (envelope.is_null())
            envelope = errorEnvelope ("host_failed", "the command produced no result");

        reply (exitCode, envelope, log);
    }

    /** Message thread. Puts the verb's result on the live document as one
        undoable step - unless the user changed it while we worked. */
    void applyStaged (const juce::String& verb, const json& liveStateAtStart,
                      json staged, int& exitCode, json& envelope)
    {
        json liveStateNow;
        host.engine->getProjectSerializer()->writeToJson (liveStateNow);

        if (documentOf (liveStateNow) != documentOf (liveStateAtStart)) {
            // Applying now would take the user's edit with it.
            exitCode = exitFailure;
            envelope = errorEnvelope ("project_changed",
                                      "the project changed in Audionaut while the command was running; "
                                      "nothing was applied - run it again");
            return;
        }

        std::string error;
        const auto applied = host.engine->getProjectFileStore()
            ->applyStateAsUndoableReload (std::move (staged), true, true,
                                          "Agent: " + verb,
                                          [&error] (std::string message) { error = message; });

        if (! applied) {
            exitCode = exitFailure;
            envelope = errorEnvelope ("apply_failed",
                                      error.empty() ? "could not apply the change to the open document" : error);
            return;
        }

        if (host.onProjectMutated != nullptr)
            host.onProjectMutated();
    }

    /** The state minus what moves without the user editing anything - the
        playhead while playing, scroll and zoom - and which applying keeps
        live anyway. */
    static json documentOf (json state)
    {
        if (auto audium = state.find ("audium"); audium != state.end() && audium->is_object()) {
            audium->erase ("scheduler");
            audium->erase ("ui_state");
        }
        return state;
    }

    AgentHost& host;
};

// ============================================================================

class AgentHost::Server final : public juce::InterprocessConnectionServer
{
public:
    Server (AgentHost& host_) : host (host_) {}

    ~Server() override { stop(); }

private:
    juce::InterprocessConnection* createConnectionObject() override
    {
        auto connection = std::make_unique<Connection> (host);
        auto* raw = connection.get();
        connections.push_back (std::move (connection));

        // drop anything the client already closed
        connections.erase (std::remove_if (connections.begin(), connections.end(),
                                           [raw] (const std::unique_ptr<Connection>& c) {
                                               return c.get() != raw && ! c->isConnected();
                                           }),
                           connections.end());
        return raw;
    }

    AgentHost& host;
    std::vector<std::unique_ptr<Connection>> connections;
};

// ============================================================================

AgentHost::AgentHost (std::shared_ptr<AudiumEngine> engine_,
                      MessageThreadExecutor messageThreadExecutor_) :
    engine (std::move (engine_)),
    messageThreadExecutor (std::move (messageThreadExecutor_))
{
    if (! messageThreadExecutor)
        messageThreadExecutor = [] (std::function<void()> task) {
            if (juce::MessageManager::getInstance()->isThisTheMessageThread())
                task();
            else
                juce::MessageManager::getInstance()->callFunctionOnMessageThread (
                    [] (void* payload) -> void* {
                        (*static_cast<std::function<void()>*> (payload))();
                        return nullptr;
                    },
                    &task);
        };
}

AgentHost::~AgentHost()
{
    stop();
}

void AgentHost::stop()
{
    if (projectFile != juce::File())
        removeHostMarker (projectFile);

    server.reset();
    projectFile = juce::File();
}

void AgentHost::setProject (const juce::File& newProjectFile)
{
    if (newProjectFile == projectFile && isHosting())
        return;

    stop();

    // A never-saved project has no package to publish a marker in.
    if (newProjectFile == juce::File() || ! newProjectFile.existsAsFile())
        return;

    auto candidate = std::make_unique<Server>(*this);

    // port 0 asks the OS for a free one; loopback only, so nothing off this
    // machine can reach it and no firewall prompt is warranted
    if (! candidate->beginWaitingForSocket (0, "127.0.0.1")) {
        if (onBindFailed != nullptr)
            onBindFailed (newProjectFile);
        return;
    }

    const auto port = candidate->getBoundPort();

    std::string error;
    if (port <= 0 || ! writeHostMarker (newProjectFile, port, error)) {
        // Without a published marker no client can find us, and a client that
        // cannot find us writes the file - so this counts as a bind failure.
        if (onBindFailed != nullptr)
            onBindFailed (newProjectFile);
        return;
    }

    server = std::move (candidate);
    projectFile = newProjectFile;
}

AgentHost::Refusal AgentHost::refusalFor (const juce::String& verb) const
{
    if (engine->getAudioBusInterface()->anyChannelRecording())
        return { "recording_in_progress",
                 "Audionaut is recording; applying a change would stop the take" };

    // The app's export and stem separation render on a worker thread while
    // their progress window's modal loop keeps serving us; a verb applied now
    // would rebuild the graph that render is walking (an `export` would even
    // run a second bounce through the same scheduler).
    if (engine->getPlayListScheduler()->isOfflineRendering())
        return { "render_in_progress",
                 "Audionaut is exporting; wait for it to finish, then run the command again" };

    // RenderTiming's offline flag is process-wide: a bounce would stretch the
    // live engine's read-ahead timeout from milliseconds to seconds while the
    // user is listening.
    if (verb == "export" && engine->getPlayListScheduler()->isPlaying())
        return { "transport_busy",
                 "Audionaut is playing; stop the transport before exporting" };

    return {};
}

juce::File AgentHost::hostedProject() const
{
    return projectFile;
}

int AgentHost::boundPort() const
{
    return server != nullptr ? server->getBoundPort() : 0;
}

} // namespace agent
} // namespace cli
} // namespace audium
