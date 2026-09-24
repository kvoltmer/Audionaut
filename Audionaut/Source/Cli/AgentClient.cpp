//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Cli/AgentClient.h"
#include "Cli/AgentProtocol.h"
#include "Cli/Commands/Commands.h"

namespace audium {
namespace cli {
namespace agent {

namespace {

constexpr int connectTimeoutMs = 2000;
constexpr int helloTimeoutMs   = 2000;

/** No reply timeout: a hosted verb may legitimately take minutes. A host that
    dies takes the connection with it, which is what we wait on instead. */
class ClientConnection final : public juce::InterprocessConnection
{
public:
    ClientConnection() : juce::InterprocessConnection (false) {}

    ~ClientConnection() override { disconnect(); }

    void connectionMade() override {}

    void connectionLost() override
    {
        lost = true;
        helloArrived.signal();
        resultArrived.signal();
    }

    void messageReceived (const juce::MemoryBlock& block) override
    {
        json message;
        if (! decode (block, message))
            return;

        const auto kind = message.value ("kind", std::string());

        if (kind == "hello") {
            hello = message;
            helloArrived.signal();
        }
        else if (kind == "result") {
            result = message;
            resultArrived.signal();
        }
    }

    bool waitForHello()  { return helloArrived.wait (helloTimeoutMs) && ! lost; }
    bool waitForResult() { resultArrived.wait(); return ! result.is_null(); }

    json hello;
    json result;

private:
    juce::WaitableEvent helloArrived;
    juce::WaitableEvent resultArrived;
    std::atomic<bool> lost { false };
};

/** Renders an envelope the host produced through our own context, so the
    caller cannot tell a hosted run from a local one. */
int renderEnvelope (const json& envelope, int exitCode, CliContext& context)
{
    if (envelope.value ("ok", false)) {
        context.ok (envelope.value ("result", json::object()));
        return exitCode;
    }

    const auto error = envelope.value ("error", json::object());
    return context.fail (exitCode != exitOk ? exitCode : exitFailure,
                         error.value ("code", std::string ("host_failed")),
                         error.value ("message", std::string ("the command failed")));
}

int refuse (CliContext& context, const std::string& message)
{
    return context.fail (exitFailure, "host_unavailable", message);
}

} // namespace

bool isHostableVerb (const juce::String& verb)
{
    // Only read-only verbs so far. Verbs that change the project need a
    // scratch engine to run on and the guards that come with it.
    return verb == "info";
}

RouteOutcome routeCommand (const juce::ArgumentList& args,
                           const juce::String& verb,
                           bool verbIsHostable,
                           CliContext& context)
{
    if (routingDisabled() || ! verbIsHostable)
        return {};

    const auto projectFile = resolveProjectFile (args);
    if (projectFile == juce::File())
        return {};

    const auto marker = readHostMarker (projectFile);
    if (! marker.has_value())
        return {}; // nobody holds it - the file is ours to read

    // From here the app holds this project, so every path must end in an
    // answer. Returning "not handled" would send the verb to the file.
    ClientConnection connection;

    if (! connection.connectToSocket ("127.0.0.1", marker->port, connectTimeoutMs))
        return { true, refuse (context, "Audionaut is holding this project but did not answer on its "
                                        "published port. Refusing to use the project file, which would "
                                        "discard whatever is unsaved.") };

    if (! connection.waitForHello())
        return { true, refuse (context, "Audionaut is holding this project but did not identify itself. "
                                        "Refusing to use the project file.") };

    if (! helloMatches (connection.hello, projectFile))
        return { true, refuse (context, "the process holding this project is not an Audionaut this "
                                        "version can talk to. Refusing to use the project file.") };

    juce::StringArray argv;
    for (auto& argument : args.arguments)
        argv.add (argument.text);

    connection.sendMessage (toBlock (makeCommand (projectFile, workingDirectory(), argv)));

    if (! connection.waitForResult())
        return { true, refuse (context, "Audionaut stopped responding while running the command. "
                                        "Refusing to use the project file.") };

    const auto envelope = connection.result.value ("envelope", json());
    const auto exitCode = connection.result.value ("exitCode", exitFailure);

    for (auto& line : connection.result.value ("log", json::array()))
        if (line.is_string())
            context.log (juce::String (line.get<std::string>()));

    if (! envelope.is_object())
        return { true, refuse (context, "Audionaut sent a reply this version could not read.") };

    return { true, renderEnvelope (envelope, exitCode, context) };
}

} // namespace agent
} // namespace cli
} // namespace audium
