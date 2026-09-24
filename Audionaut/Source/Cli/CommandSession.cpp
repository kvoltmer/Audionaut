//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <optional>

#include "Cli/CommandSession.h"
#include "Cli/HeadlessEngineSession.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Project/ProjectFileStore.h"

namespace audium {
namespace cli {

namespace {

/** The live engine a host lent us, while a HostedSessionScope stands. */
struct HostedContext
{
    std::shared_ptr<AudiumEngine> engine;
    juce::File projectFile;
};

HostedContext& hostedContext()
{
    static HostedContext context;
    return context;
}

} // namespace

struct ProjectSession::Impl
{
    /** File mode: owns an engine of its own and opens the project into it. */
    Impl (juce::File projectFile_, CommandAccess access_) :
        projectFile (std::move (projectFile_)),
        access (access_)
    {
        engineSession.emplace();
    }

    /** Hosted: borrows the live engine; there is nothing to open or save. */
    Impl (std::shared_ptr<AudiumEngine> borrowed, juce::File projectFile_) :
        borrowedEngine (std::move (borrowed)),
        projectFile (std::move (projectFile_)),
        access (CommandAccess::readOnly),
        hosted (true)
    {
    }

    std::shared_ptr<AudiumEngine> engine() const
    {
        return hosted ? borrowedEngine : engineSession->get();
    }

    std::optional<HeadlessEngineSession> engineSession;
    std::shared_ptr<AudiumEngine> borrowedEngine;
    juce::File projectFile;
    CommandAccess access;
    bool hosted = false;
};

HostedSessionScope::HostedSessionScope (std::shared_ptr<AudiumEngine> liveEngine, juce::File projectFile)
{
    hostedContext() = { std::move (liveEngine), std::move (projectFile) };
}

HostedSessionScope::~HostedSessionScope()
{
    hostedContext() = {};
}

bool isHostedExecution()
{
    return hostedContext().engine != nullptr;
}

ProjectSession::ProjectSession() noexcept = default;
ProjectSession::ProjectSession (ProjectSession&&) noexcept = default;
ProjectSession& ProjectSession::operator= (ProjectSession&&) noexcept = default;
ProjectSession::~ProjectSession() = default;

ProjectSession::ProjectSession (std::unique_ptr<Impl> impl_) noexcept :
    impl (std::move (impl_))
{
}

AudiumEngine& ProjectSession::operator*() const
{
    jassert (impl != nullptr);
    return *impl->engine();
}

AudiumEngine* ProjectSession::operator->() const
{
    jassert (impl != nullptr);
    return impl->engine().get();
}

std::shared_ptr<AudiumEngine> ProjectSession::get() const
{
    jassert (impl != nullptr);
    return impl->engine();
}

bool ProjectSession::commit (std::string& error)
{
    jassert (impl != nullptr);

    if (impl->hosted) {
        // the document belongs to the app that lent us the engine; only the
        // user saves it
        error = "the project is open in Audionaut and is the user's to save";
        return false;
    }

    if (impl->access != CommandAccess::mutating) {
        // a read-only verb reaching this line is a programming error, but
        // refusing beats writing the project by accident
        error = "this command is not allowed to change the project";
        return false;
    }

    return (*impl->engineSession)->getProjectFileStore()
               ->save (impl->projectFile, [&error] (std::string message) { error = message; });
}

bool ProjectSession::isHosted() const noexcept
{
    return impl != nullptr && impl->hosted;
}

juce::File ProjectSession::projectFile() const
{
    jassert (impl != nullptr);
    return impl->projectFile;
}

ProjectSession openProjectSession (const juce::File& projectFile,
                                   CommandAccess access,
                                   CliContext& context,
                                   int& failureExitCode,
                                   const std::function<void (AudiumEngine&)>& configure)
{
    // A host running this verb lends us the document the user has open, so
    // there is nothing to read from disk.
    if (isHostedExecution())
        return ProjectSession (std::make_unique<ProjectSession::Impl> (hostedContext().engine,
                                                                       hostedContext().projectFile));

    auto impl = std::make_unique<ProjectSession::Impl> (projectFile, access);

    if (configure)
        configure (*impl->engineSession->get());

    std::string error;
    if (! (*impl->engineSession)->getProjectFileStore()
              ->open (projectFile, [&error] (std::string message) { error = message; })) {
        failureExitCode = context.fail (exitFailure, "open_failed",
                                        error.empty() ? "failed to open project" : error);
        return {};
    }

    return ProjectSession (std::move (impl));
}

ProjectSession openEngineSession()
{
    return ProjectSession (std::make_unique<ProjectSession::Impl> (juce::File(), CommandAccess::readOnly));
}

} // namespace cli
} // namespace audium
