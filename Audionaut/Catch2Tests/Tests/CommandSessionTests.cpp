#include <catch2/catch_test_macros.hpp>

#include "Cli/CommandSession.h"
#include "Cli/Commands/Commands.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Engine/Group/AudioTrackContainer.h"

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
