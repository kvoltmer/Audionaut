//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <optional>
#include <string>
#include <JuceHeader.h>
#include <nlohmann/json.hpp>

namespace audium {
namespace cli {
namespace agent {

using json = nlohmann::json;

/**
 * The wire contract between a CLI/MCP invocation and a running app that holds
 * the project it addresses.
 *
 * The app writes `Project.json` only when the user saves, so a verb that goes
 * to the file sees the project as last saved - and saving its own work back
 * overwrites a document the user never asked to have written. When the app
 * holds the project, the verb is handed to it instead: state is serialized
 * from the live graph on demand and edits stay in memory until the user saves.
 *
 * Discovery is a marker file rather than a derived port. A port derived from
 * the project path can be occupied by an unrelated process, and then the app
 * holds the project, nobody answers, and the CLI writes the file anyway -
 * exactly the failure this exists to prevent. The marker says who to talk to,
 * and its pid says whether they are still there.
 */

/** Bumped when the message shapes change; the host serves anything <= its own. */
constexpr int protocolVersion = 1;

/** Identifies our own host, so a stale pid belonging to something else is caught. */
constexpr const char* appIdentifier = "Audionaut";

/** The marker a hosting app leaves beside Project.json. */
constexpr const char* hostMarkerFileName = "Host.json";

/** Set to "0" to make every verb ignore a running host and use the file. */
constexpr const char* routingDisabledEnvVar = "AUDIONAUT_AGENT_ROUTING";

/** What a hosting app published about itself. */
struct HostMarker
{
    int protocol = 0;
    int pid = 0;
    int port = 0;
    juce::File project;
};

/** @brief The .audium package holding `projectFileOrPackage`, or the file's own
           directory when it is a loose Project.json. */
juce::File projectPackageFor (const juce::File& projectFileOrPackage);

/** @brief The marker's location for a project file or package. */
juce::File markerFileFor (const juce::File& projectFileOrPackage);

/**
 * @brief Publishes a marker for `projectFile` on `port`.
 * @return False (with `error` set) when the marker could not be written - the
 *         caller must then not consider itself to be hosting.
 */
bool writeHostMarker (const juce::File& projectFile, int port, std::string& error);

/** @brief Removes any marker beside `projectFile`. */
void removeHostMarker (const juce::File& projectFile);

/**
 * @brief Reads the marker beside `projectFile`, if a live host left one.
 *
 * Returns nothing when there is no marker, it does not parse, or its pid is
 * gone - a crashed app leaves its marker behind, so the pid is what separates
 * a host from a leftover. A stale marker whose pid has been recycled survives
 * this check and is caught by the handshake instead.
 */
std::optional<HostMarker> readHostMarker (const juce::File& projectFile);

/** @brief Whether routing is switched off for this process. */
bool routingDisabled();

// ==== message shapes ========================================================

/** @brief The greeting a host sends as soon as a client connects. */
json makeHello (const juce::File& project, int hostPid);

/** @brief A verb and the client's working directory, for the host to run. */
json makeCommand (const juce::File& project, const juce::File& workingDirectory,
                  const juce::StringArray& argv);

/** @brief A finished verb: its exit code, its result envelope and its log. */
json makeResult (int exitCode, const json& envelope, const juce::StringArray& log);

/**
 * @brief Whether `hello` came from a host of ours serving `project`.
 *
 * Guards against a recycled pid, a different application answering on the
 * port, and a host too new to understand us.
 */
bool helloMatches (const json& hello, const juce::File& project);

json encode (const json& message);
bool decode (const juce::MemoryBlock& block, json& message);
juce::MemoryBlock toBlock (const json& message);

} // namespace agent
} // namespace cli
} // namespace audium
