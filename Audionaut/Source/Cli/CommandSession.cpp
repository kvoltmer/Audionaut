//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <optional>

#include "Cli/CommandSession.h"
#include "Cli/HeadlessEngineSession.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Engine/Project/ProjectSerializer.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Analysis/AnalysisProvider.h"
#include "Engine/Analysis/AnalysisCache.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/Playback/AudioBusInterface.h"
#include "Engine/PlayList/PlayListScheduler.h"

namespace audium {
namespace cli {

namespace {

/** The live engine a host lent us, while a HostedSessionScope stands. */
struct HostedContext
{
    std::shared_ptr<AudiumEngine> engine;
    juce::File projectFile;
    json liveState;
    json stagedState;
    bool staged = false;
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

    /** Hosted read: borrows the live engine; there is nothing to open or save. */
    Impl (std::shared_ptr<AudiumEngine> borrowed, juce::File projectFile_) :
        borrowedEngine (std::move (borrowed)),
        projectFile (std::move (projectFile_)),
        access (CommandAccess::readOnly),
        hosted (true)
    {
    }

    /** Hosted, on an engine of its own, seeded from the live state. */
    Impl (const json& seedState, juce::File projectFile_, bool& seededOk, CommandAccess access_) :
        projectFile (std::move (projectFile_)),
        access (access_),
        hosted (true),
        scratch (true)
    {
        // The scratch engine lives inside a process that already runs a
        // MessageManager and owns the session's temp directory - it must
        // borrow both rather than create and then destroy them.
        HeadlessEngineSession::setUseExternalMessageManager (true);
        engineSession.emplace();
        HeadlessEngineSession::setUseExternalMessageManager (false);

        auto engineRef = engineSession->get();
        engineRef->getAudioResourceContainer()->setOwnsTemporaryDirectory (false);

        // No device drains this engine's command fifo, so it pumps its own -
        // the live engine's device is draining as usual and must not.
        engineRef->getAudioBusInterface()->setPumpsCommandsSynchronously (true);

        // Resource paths resolve against the package, which the live session
        // already points the statics at; the analysis cache has to be loaded
        // for the verbs that read it.
        engineRef->getAudioTrackContainer()->getAnalysisProvider()->getCache()
                 ->loadFromFolder (ProjectFileStore::projectDirectory);

        json seed = seedState;
        try {
            seededOk = engineRef->getProjectSerializer()->readFromJson (seed, true);
        }
        catch (const std::exception&) {
            seededOk = false;
        }

        // readFromJson only rebuilds the graph; opening a project also hands
        // the playlist to the scheduler, and without that there is nothing for
        // a render to play - an export would write an empty file.
        if (seededOk)
            engineRef->getPlayListScheduler()->commitPlayListData();
    }

    std::shared_ptr<AudiumEngine> engine() const
    {
        return borrowedEngine != nullptr ? borrowedEngine : engineSession->get();
    }

    std::optional<HeadlessEngineSession> engineSession;
    std::shared_ptr<AudiumEngine> borrowedEngine;
    juce::File projectFile;
    CommandAccess access;
    bool hosted = false;
    bool scratch = false;
};

HostedSessionScope::HostedSessionScope (std::shared_ptr<AudiumEngine> liveEngine,
                                        juce::File projectFile,
                                        json liveState)
{
    hostedContext() = { std::move (liveEngine), std::move (projectFile), std::move (liveState), {}, false };
}

bool HostedSessionScope::hasStagedState()
{
    return hostedContext().staged;
}

json HostedSessionScope::takeStagedState()
{
    auto state = std::move (hostedContext().stagedState);
    hostedContext().stagedState = {};
    hostedContext().staged = false;
    return state;
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

    if (impl->scratch && impl->access == CommandAccess::mutating) {
        // Hand the result to the host, which applies it to the live document
        // as one undoable step. Nothing of ours reaches the project file - the
        // user still decides when their document is written.
        json after;
        impl->engine()->getProjectSerializer()->writeToJson (after);
        hostedContext().stagedState = std::move (after);
        hostedContext().staged = true;
        return true;
    }

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
    if (isHostedExecution()) {
        // Only a plain read may touch the live graph. A bounce prepares the
        // scheduler and flips the process-wide offline-render flag, so it gets
        // its own engine even though it changes nothing.
        if (access == CommandAccess::readOnly)
            return ProjectSession (std::make_unique<ProjectSession::Impl> (hostedContext().engine,
                                                                           hostedContext().projectFile));

        auto seeded = false;
        auto scratchImpl = std::make_unique<ProjectSession::Impl> (hostedContext().liveState,
                                                                   hostedContext().projectFile, seeded,
                                                                   access);
        if (! seeded) {
            failureExitCode = context.fail (exitFailure, "host_failed",
                                            "could not copy the open document to work on");
            return {};
        }

        if (configure)
            configure (*scratchImpl->engine());

        return ProjectSession (std::move (scratchImpl));
    }

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
