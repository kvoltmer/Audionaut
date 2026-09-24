//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <memory>
#include <JuceHeader.h>

namespace audium {

class AudiumEngine;

namespace cli {
namespace agent {

/**
 * @class AgentHost
 * @brief Serves CLI verbs against the document this app has open.
 *
 * The app writes `Project.json` only when the user saves, so a verb that goes
 * to the file works from the last save and writes its result back over a
 * document the user never asked to have written. While this host is up, such
 * a verb is handed here instead: it runs against the live graph, so it reads
 * what the user can see, and nothing reaches the project file.
 *
 * Presence is published as a marker beside `Project.json` (see AgentProtocol),
 * and withdrawn as soon as the project changes or the app goes away.
 *
 * Only read-only verbs are served so far. Verbs that change the project need
 * a scratch engine and the guards that come with it.
 */
class AgentHost
{
public:
    /**
     * @param engine_ The live engine; verbs are run against this graph.
     * @param messageThreadExecutor_ Runs a task on the message thread and
     *        waits for it. Injectable because the default blocks on a running
     *        dispatch loop, which a test process does not have.
     */
    using MessageThreadExecutor = std::function<void (std::function<void()>)>;

    explicit AgentHost (std::shared_ptr<AudiumEngine> engine_,
                        MessageThreadExecutor messageThreadExecutor_ = {});
    ~AgentHost();

    /**
     * @brief Starts (or moves) hosting to `projectFile`, or stops it when the
     *        file is invalid - a never-saved project has nowhere to publish.
     *
     * Safe to call repeatedly with the same project.
     */
    void setProject (const juce::File& projectFile);

    /** @brief Called after a hosted verb changed the document, so the app can
        refresh its title and views. */
    std::function<void()> onProjectMutated;

    /** @brief Reported when a port could not be bound, so the app can say that
        agent access is unavailable rather than leave it silently broken. */
    std::function<void (const juce::File& projectFile)> onBindFailed;

    /** @brief The project currently served, if any. */
    juce::File hostedProject() const;

    /** @brief The bound port, or 0 when not hosting. */
    int boundPort() const;

    bool isHosting() const { return boundPort() != 0; }

private:
    class Server;
    class Connection;

    void stop();

    /**
     * @brief Why this verb cannot run right now, or empty when it can.
     *
     * Message thread. Recording is refused outright - applying a state stops
     * the take. Export is refused while the transport plays, because the
     * offline-render flag it sets is process-wide and would stretch the live
     * engine's read-ahead timeout underneath the user.
     */
    std::string refusalFor (const juce::String& verb) const;

    std::atomic<bool> busy { false };

    std::shared_ptr<AudiumEngine> engine;
    MessageThreadExecutor messageThreadExecutor;
    std::unique_ptr<Server> server;
    juce::File projectFile;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AgentHost)
};

} // namespace agent
} // namespace cli
} // namespace audium
