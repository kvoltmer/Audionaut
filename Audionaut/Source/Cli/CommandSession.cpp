//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Cli/CommandSession.h"
#include "Cli/HeadlessEngineSession.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Project/ProjectFileStore.h"

namespace audium {
namespace cli {

struct ProjectSession::Impl
{
    Impl (juce::File projectFile_, CommandAccess access_) :
        projectFile (std::move (projectFile_)),
        access (access_)
    {
    }

    HeadlessEngineSession engineSession;
    juce::File projectFile;
    CommandAccess access;
};

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
    return *impl->engineSession.get();
}

AudiumEngine* ProjectSession::operator->() const
{
    jassert (impl != nullptr);
    return impl->engineSession.get().get();
}

std::shared_ptr<AudiumEngine> ProjectSession::get() const
{
    jassert (impl != nullptr);
    return impl->engineSession.get();
}

bool ProjectSession::commit (std::string& error)
{
    jassert (impl != nullptr);

    if (impl->access != CommandAccess::mutating) {
        // a read-only verb reaching this line is a programming error, but
        // refusing beats writing the project by accident
        error = "this command is not allowed to change the project";
        return false;
    }

    return impl->engineSession->getProjectFileStore()
               ->save (impl->projectFile, [&error] (std::string message) { error = message; });
}

bool ProjectSession::isHosted() const noexcept
{
    return false;
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
    auto impl = std::make_unique<ProjectSession::Impl> (projectFile, access);

    if (configure)
        configure (*impl->engineSession.get());

    std::string error;
    if (! impl->engineSession->getProjectFileStore()
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
