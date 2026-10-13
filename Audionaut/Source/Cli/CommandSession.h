//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <JuceHeader.h>

#include "Cli/CliContext.h"
#include "Engine/AudiumEngine.h"

namespace audium {
namespace cli {

/**
 * @brief What a verb intends to do with the project it opens.
 *
 * `commit()` is refused for anything but `mutating`, so a read-only verb
 * cannot write the project by accident.
 */
enum class CommandAccess
{
    readOnly, ///< reads the project, and may do so on a live document directly
    isolated, ///< reads the project but needs an engine of its own (export)
    mutating  ///< changes the project; `commit()` persists the work
};

/**
 * @class ProjectSession
 * @brief The engine a verb runs against, plus the two lifecycle steps the
 *        verbs used to perform for themselves.
 *
 * Verbs used to open the project file, mutate the graph and save it back. That
 * shape hard-codes *where* the project lives, which is why an agent editing a
 * project the GUI has open overwrites the user's document. Separating the
 * session from the operation leaves the body of every verb addressing an
 * engine, and lets the session decide what opening and committing mean.
 *
 * Today there is one implementation and it behaves exactly as before: open the
 * file, save the file. The point is the seam.
 *
 * Deliberately shaped like `HeadlessEngineSession` - `operator->` reaches the
 * engine - so the verbs read the same either way.
 */
class ProjectSession
{
public:
    ProjectSession() noexcept;
    ProjectSession (ProjectSession&&) noexcept;
    ProjectSession& operator= (ProjectSession&&) noexcept;
    ~ProjectSession();

    ProjectSession (const ProjectSession&) = delete;
    ProjectSession& operator= (const ProjectSession&) = delete;

    /** @brief False when the session could not be opened. */
    explicit operator bool() const noexcept { return impl != nullptr; }

    AudiumEngine& operator*() const;
    AudiumEngine* operator->() const;
    std::shared_ptr<AudiumEngine> get() const;

    /**
     * @brief Persists the verb's work, and reports why if it could not.
     * @return False on failure, or when the verb declared itself read-only.
     */
    bool commit (std::string& error);

    /** @brief Whether the work is being applied to a project someone else owns. */
    bool isHosted() const noexcept;

    /** @brief The project file this session addresses. */
    juce::File projectFile() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;

    explicit ProjectSession (std::unique_ptr<Impl> impl_) noexcept;

    friend ProjectSession openProjectSession (const juce::File&, CommandAccess, CliContext&, int&,
                                              const std::function<void (AudiumEngine&)>&);
    friend ProjectSession openEngineSession();
};

/**
 * @brief Opens `projectFile` for a verb, reporting failure through `context`.
 * @param failureExitCode Receives the exit code to return when the session is
 *        empty; untouched on success.
 * @param configure Runs on the engine after it is built and before the project
 *        is opened - the only window for settings that must be in place for
 *        the open itself (import turns auto-analysis off here, or opening the
 *        project would queue every file in it).
 * @return An empty session on failure - test it before use.
 */
ProjectSession openProjectSession (const juce::File& projectFile,
                                   CommandAccess access,
                                   CliContext& context,
                                   int& failureExitCode,
                                   const std::function<void (AudiumEngine&)>& configure = {});

/**
 * @brief A session with an engine but no project, for a verb that can also
 *        work on a bare audio file (`analyze`). `commit()` is refused.
 */
ProjectSession openEngineSession();

/**
 * @class HostedSessionScope
 * @brief Makes `openProjectSession` hand out the live engine for its lifetime.
 *
 * Installed by the host around a verb it is running for a client. While it
 * stands, the verb addresses the document the user has open instead of the
 * file on disk - so it reads what they can see, and `commit()` is refused
 * because the project file is theirs to write, not ours.
 */
class HostedSessionScope
{
public:
    /**
     * @param liveEngine The document the user has open.
     * @param projectFile Where that document lives, for verbs that need a path.
     * @param liveState The live graph, serialized. A mutating verb runs on a
     *        scratch engine seeded from this rather than on the live one, so a
     *        verb that fails leaves the user's document untouched and the whole
     *        change arrives as a single undoable step.
     */
    HostedSessionScope (std::shared_ptr<AudiumEngine> liveEngine,
                        juce::File projectFile,
                        nlohmann::json liveState);
    ~HostedSessionScope();

    HostedSessionScope (const HostedSessionScope&) = delete;
    HostedSessionScope& operator= (const HostedSessionScope&) = delete;

    /** @brief Whether a verb committed work that is waiting to be applied. */
    static bool hasStagedState();

    /** @brief The state a verb committed, and clears it. */
    static nlohmann::json takeStagedState();
};

/** @brief Whether a verb is currently running inside a `HostedSessionScope`. */
bool isHostedExecution();

} // namespace cli
} // namespace audium
