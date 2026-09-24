//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Cli/AgentProtocol.h"
#include "Engine/Project/ProjectFileStore.h"

#if JUCE_WINDOWS
 #include <windows.h>
#else
 #include <unistd.h>
#endif

namespace audium {
namespace cli {
namespace agent {

namespace {

int currentProcessId()
{
#if JUCE_WINDOWS
    return (int) GetCurrentProcessId();
#else
    return (int) getpid();
#endif
}

/** Both ends must agree on one spelling of a package path. */
juce::String canonicalKey (const juce::File& projectFileOrPackage)
{
    return projectPackageFor (projectFileOrPackage).getFullPathName().trimCharactersAtEnd ("/\\");
}

} // namespace

juce::File projectPackageFor (const juce::File& projectFileOrPackage)
{
    if (projectFileOrPackage.isDirectory())
        return projectFileOrPackage;

    return projectFileOrPackage.getParentDirectory();
}

juce::File markerFileFor (const juce::File& projectFileOrPackage)
{
    return projectPackageFor (projectFileOrPackage).getChildFile (hostMarkerFileName);
}

bool writeHostMarker (const juce::File& projectFile, int port, std::string& error)
{
    const auto package = projectPackageFor (projectFile);

    if (! package.isDirectory()) {
        error = "no project package to publish the marker in";
        return false;
    }

    json marker = { { "protocol", protocolVersion },
                    { "app",      appIdentifier },
                    { "pid",      currentProcessId() },
                    { "port",     port },
                    { "project",  canonicalKey (projectFile).toStdString() } };

    if (! markerFileFor (projectFile).replaceWithText (marker.dump (2))) {
        error = "could not write " + std::string (hostMarkerFileName);
        return false;
    }

    return true;
}

void removeHostMarker (const juce::File& projectFile)
{
    markerFileFor (projectFile).deleteFile();
}

std::optional<HostMarker> readHostMarker (const juce::File& projectFile)
{
    const auto file = markerFileFor (projectFile);

    if (! file.existsAsFile())
        return {};

    try {
        const auto parsed = json::parse (file.loadFileAsString().toStdString());

        HostMarker marker;
        marker.protocol = parsed.value ("protocol", 0);
        marker.pid      = parsed.value ("pid", 0);
        marker.port     = parsed.value ("port", 0);
        marker.project  = juce::File (juce::String (parsed.value ("project", std::string())));

        if (parsed.value ("app", std::string()) != appIdentifier)
            return {};

        if (marker.port <= 0 || marker.pid <= 0)
            return {};

        // A crashed app leaves its marker behind; the pid is what separates a
        // live host from the leftovers.
        if (! ProjectFileStore::isProcessAlive (marker.pid))
            return {};

        return marker;
    }
    catch (const std::exception&) {
        return {};
    }
}

bool routingDisabled()
{
    return juce::SystemStats::getEnvironmentVariable (routingDisabledEnvVar, "") == "0";
}

json makeHello (const juce::File& project, int hostPid)
{
    return { { "protocol",   protocolVersion },
             { "kind",       "hello" },
             { "app",        appIdentifier },
             { "appVersion", ProjectInfo::versionString },
             { "project",    canonicalKey (project).toStdString() },
             { "hostPid",    hostPid } };
}

json makeCommand (const juce::File& project, const juce::File& workingDirectory,
                  const juce::StringArray& argv)
{
    auto arguments = json::array();
    for (auto& argument : argv)
        arguments.push_back (argument.toStdString());

    return { { "protocol", protocolVersion },
             { "kind",     "command" },
             { "project",  canonicalKey (project).toStdString() },
             { "cwd",      workingDirectory.getFullPathName().toStdString() },
             { "argv",     arguments } };
}

json makeResult (int exitCode, const json& envelope, const juce::StringArray& log)
{
    auto lines = json::array();
    for (auto& line : log)
        lines.push_back (line.toStdString());

    return { { "protocol", protocolVersion },
             { "kind",     "result" },
             { "exitCode", exitCode },
             { "envelope", envelope },
             { "log",      lines } };
}

bool helloMatches (const json& hello, const juce::File& project)
{
    if (! hello.is_object())
        return false;

    if (hello.value ("kind", std::string()) != "hello")
        return false;

    if (hello.value ("app", std::string()) != appIdentifier)
        return false;

    // A host that speaks a newer protocol than we do is not ours to talk to;
    // the caller reports that rather than falling back to writing the file.
    if (hello.value ("protocol", 0) > protocolVersion)
        return false;

    return hello.value ("project", std::string()) == canonicalKey (project).toStdString();
}

json encode (const json& message)
{
    return message;
}

bool decode (const juce::MemoryBlock& block, json& message)
{
    try {
        message = json::parse (std::string (static_cast<const char*> (block.getData()), block.getSize()));
        return message.is_object();
    }
    catch (const std::exception&) {
        return false;
    }
}

juce::MemoryBlock toBlock (const json& message)
{
    const auto text = message.dump();
    return juce::MemoryBlock (text.data(), text.size());
}

} // namespace agent
} // namespace cli
} // namespace audium
