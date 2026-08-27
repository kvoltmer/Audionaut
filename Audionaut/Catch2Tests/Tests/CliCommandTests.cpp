#include <catch2/catch_test_macros.hpp>

#include "Cli/Commands/Commands.h"
#include "Engine/ProjectFileStore.h"
#include "Engine/AudiumEngine.h"

using namespace audium;

// The run* functions own the whole headless lifecycle (MessageManager,
// engine, teardown) per invocation, so these tests must not hold their own
// MessageManager or engine across calls - they drive the commands exactly
// like main() does and inspect the results on disk.

namespace {

juce::ArgumentList makeArgs (const juce::String& commandLine)
{
    return juce::ArgumentList ("audionaut-cli", commandLine);
}

juce::File makeWorkDirectory()
{
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                   .getChildFile ("audionaut-cli-tests");
    dir.deleteRecursively();
    REQUIRE (dir.createDirectory());
    return dir;
}

const auto testFilesDir = juce::String (CURRENT_SOURCE_DIR) + "/TestFiles/";

} // namespace

SCENARIO ("cli option parsing", "[cli]")
{
    GIVEN ("a command line with = and space separated option values") {
        auto args = makeArgs ("create /tmp/foo.audium --channels 4 --mode=random rest");

        WHEN ("the options are taken") {
            auto channels = cli::takeOptionValue (args, "--channels");
            auto mode = cli::takeOptionValue (args, "--mode");

            THEN ("both forms yield their value and only plain args remain") {
                REQUIRE (channels == "4");
                REQUIRE (mode == "random");

                auto plain = cli::getPlainArguments (args);
                REQUIRE (plain.size() == 2);
                REQUIRE (plain[0] == "/tmp/foo.audium");
                REQUIRE (plain[1] == "rest");
            }
        }
    }
}

SCENARIO ("cli create and info", "[cli]")
{
    auto workDir = makeWorkDirectory();
    auto project = workDir.getChildFile ("created.audium");

    cli::CliContext context;
    context.quiet = true;

    GIVEN ("a create invocation") {
        auto exitCode = cli::runCreate (makeArgs ("create " + project.getFullPathName() + " --channels 2"), context);

        THEN ("a valid project package exists") {
            REQUIRE (exitCode == cli::exitOk);
            REQUIRE (ProjectFileStore::isValidProjectStructure (project));
        }

        WHEN ("info runs on it") {
            REQUIRE (cli::runInfo (makeArgs ("info " + project.getFullPathName()), context) == cli::exitOk);
        }

        WHEN ("create runs again on the same path") {
            THEN ("it refuses to overwrite") {
                REQUIRE (cli::runCreate (makeArgs ("create " + project.getFullPathName()), context)
                         == cli::exitFailure);
            }
        }
    }

    GIVEN ("a missing project") {
        THEN ("info fails with a usage error") {
            REQUIRE (cli::runInfo (makeArgs ("info " + workDir.getChildFile ("nope.audium").getFullPathName()),
                                   context)
                     == cli::exitUsage);
        }
    }

    workDir.deleteRecursively();
}

SCENARIO ("cli import and export round trip", "[cli]")
{
    auto workDir = makeWorkDirectory();
    auto project = workDir.getChildFile ("roundtrip.audium");
    auto outputFile = workDir.getChildFile ("bounce.wav");
    auto audioFile = juce::File (testFilesDir + "sine-0dB.wav");
    REQUIRE (audioFile.existsAsFile());

    cli::CliContext context;
    context.quiet = true;

    GIVEN ("a project with an imported sine file") {
        REQUIRE (cli::runCreate (makeArgs ("create " + project.getFullPathName() + " --channels 1"), context)
                 == cli::exitOk);
        REQUIRE (cli::runImport (makeArgs ("import " + project.getFullPathName() + " "
                                           + audioFile.getFullPathName()),
                                 context)
                 == cli::exitOk);

        WHEN ("the project is exported") {
            REQUIRE (cli::runExport (makeArgs ("export " + project.getFullPathName() + " -o "
                                               + outputFile.getFullPathName() + " --channels 1"),
                                     context)
                     == cli::exitOk);

            THEN ("the bounce contains the (non-silent) sine") {
                REQUIRE (outputFile.existsAsFile());

                juce::AudioFormatManager formatManager;
                formatManager.registerBasicFormats();
                std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (outputFile));
                REQUIRE (reader != nullptr);
                REQUIRE (reader->lengthInSamples > 0);

                juce::AudioBuffer<float> buffer (1, (int) reader->lengthInSamples);
                reader->read (&buffer, 0, (int) reader->lengthInSamples, 0, true, false);
                REQUIRE (buffer.getMagnitude (0, 0, buffer.getNumSamples()) > 0.5f);
            }
        }
    }

    workDir.deleteRecursively();
}
