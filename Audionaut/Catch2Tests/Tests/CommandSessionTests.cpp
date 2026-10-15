#include <catch2/catch_test_macros.hpp>

#include "Cli/CommandSession.h"
#include "Cli/Commands/Commands.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Analysis/AnalysisCache.h"
#include "Engine/Analysis/AnalysisWorker.h"
#include "Engine/Resource/AudioResourceContainer.h"

using namespace audium;

namespace {

juce::File makeSessionWorkDirectory()
{
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                   .getChildFile ("audionaut-session-tests");
    dir.deleteRecursively();
    REQUIRE (dir.createDirectory());
    return dir;
}

} // namespace

// ProjectSession is the seam that lets a verb's body stop caring where the
// project lives. These cover the contract the verbs now rely on; that the
// verbs themselves still behave is CliCommandTests' job.
SCENARIO("a command session hands a verb an engine and owns the lifecycle",
         "[cli][session]")
{
    auto workDir = makeSessionWorkDirectory();
    auto project = workDir.getChildFile ("session-test.audium");
    cli::CliContext context;
    context.quiet = true;

    REQUIRE (cli::runCreate (juce::ArgumentList ("audionaut-cli",
                                                 "create " + project.getFullPathName()),
                             context) == cli::exitOk);

    const auto projectJson = project.getChildFile (ProjectFileStore::projectFileName);
    REQUIRE (projectJson.existsAsFile());

    GIVEN("a mutating session on an existing project") {
        int failure = cli::exitFailure;
        auto session = cli::openProjectSession (projectJson, cli::CommandAccess::mutating,
                                                context, failure);

        THEN("it opens, reaches the engine, and reports where it writes") {
            REQUIRE (static_cast<bool> (session));
            REQUIRE (session->getAudioTrackContainer() != nullptr);
            REQUIRE (session.projectFile() == projectJson);
            REQUIRE_FALSE (session.isHosted());
        }

        WHEN("the verb commits a change") {
            session->getAudioTrackContainer()->setMasterGain (0.4f);

            std::string error;
            REQUIRE (session.commit (error));
            REQUIRE (error.empty());

            THEN("it reaches the project file") {
                auto reopened = cli::openProjectSession (projectJson, cli::CommandAccess::readOnly,
                                                         context, failure);
                REQUIRE (static_cast<bool> (reopened));
                REQUIRE (reopened->getAudioTrackContainer()->getMasterGain() == 0.4f);
            }
        }
    }

    GIVEN("a read-only session") {
        int failure = cli::exitFailure;
        auto session = cli::openProjectSession (projectJson, cli::CommandAccess::readOnly,
                                                context, failure);
        REQUIRE (static_cast<bool> (session));

        const auto writtenAt = projectJson.getLastModificationTime();

        THEN("committing is refused and the project file is left alone") {
            session->getAudioTrackContainer()->setMasterGain (0.9f);

            std::string error;
            REQUIRE_FALSE (session.commit (error));
            REQUIRE_FALSE (error.empty());
            REQUIRE (projectJson.getLastModificationTime() == writtenAt);
        }
    }

    GIVEN("a project that is not there") {
        int failure = cli::exitOk;
        auto session = cli::openProjectSession (workDir.getChildFile ("nope.audium")
                                                    .getChildFile (ProjectFileStore::projectFileName),
                                                cli::CommandAccess::mutating, context, failure);

        THEN("the session is empty and carries the exit code to return") {
            REQUIRE_FALSE (static_cast<bool> (session));
            REQUIRE (failure == cli::exitFailure);
        }
    }

    GIVEN("a session with no project at all") {
        auto session = cli::openEngineSession();

        THEN("it still hands over an engine, but will not commit") {
            REQUIRE (static_cast<bool> (session));
            REQUIRE (session->getAudioTrackContainer() != nullptr);
            REQUIRE (session.projectFile() == juce::File());

            std::string error;
            REQUIRE_FALSE (session.commit (error));
        }
    }

    workDir.deleteRecursively();
}

// Opening a project normally queues background analysis for every file the
// cache lacks. A CLI engine lives for one verb: the queue only made it wait at
// exit (seconds per file) for results a read-only verb never saves, and a
// mutating one saved whatever happened to be done by its commit.
SCENARIO("a command session does not queue background analysis",
         "[cli][session][analysis]")
{
    auto workDir = makeSessionWorkDirectory();
    auto project = workDir.getChildFile ("no-analysis.audium");
    cli::CliContext context;
    context.quiet = true;

    REQUIRE (cli::runCreate (juce::ArgumentList ("audionaut-cli", "create " + project.getFullPathName()),
                             context) == cli::exitOk);
    const auto audio = juce::String (CURRENT_SOURCE_DIR) + "/TestFiles/120-funk-1-sec.wav";
    REQUIRE (cli::runImport (juce::ArgumentList ("audionaut-cli", "import " + project.getFullPathName() + " " + audio),
                             context) == cli::exitOk);
    REQUIRE_FALSE (project.getChildFile (AnalysisCache::fileName).existsAsFile());

    const auto projectJson = project.getChildFile (ProjectFileStore::projectFileName);

    for (auto access : { cli::CommandAccess::readOnly, cli::CommandAccess::mutating }) {
        int failure = cli::exitFailure;
        auto session = cli::openProjectSession (projectJson, access, context, failure);
        REQUIRE (static_cast<bool> (session));

        auto worker = session->getAudioResourceContainer()->getAnalysisWorker();
        REQUIRE_FALSE (worker->isAutoAnalysisEnabled());
        REQUIRE (worker->getRemainingCount() == 0);
    }

    workDir.deleteRecursively();
}
