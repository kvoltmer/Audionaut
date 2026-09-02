#include <algorithm>

#include <catch2/catch_test_macros.hpp>

#include "Cli/CliDispatch.h"

using namespace audium;

namespace {

juce::ArgumentList makeArgs (const juce::String& commandLine)
{
    return juce::ArgumentList ("audionaut-cli", commandLine);
}

} // namespace

SCENARIO ("cli dispatch fall-through and matching", "[cli]")
{
    cli::CliContext context;
    context.quiet = true;

    GIVEN ("inputs that name no verb") {
        THEN ("dispatch returns the not-performed sentinel") {
            REQUIRE (cli::performCliCommand (makeArgs (""), context) == cli::cliCommandNotPerformed);
            REQUIRE (cli::performCliCommand (makeArgs ("frobnicate"), context) == cli::cliCommandNotPerformed);
            REQUIRE (cli::performCliCommand (makeArgs ("--json"), context) == cli::cliCommandNotPerformed);
            // a file path, as passed when double-clicking / `Audionaut foo.audium`
            REQUIRE (cli::performCliCommand (makeArgs ("/tmp/somewhere/foo.audium"), context)
                     == cli::cliCommandNotPerformed);
        }
    }

    GIVEN ("a known verb with unusable arguments") {
        THEN ("the command runs and reports a usage error, not the sentinel") {
            REQUIRE (cli::performCliCommand (makeArgs ("info"), context) == cli::exitUsage);
            REQUIRE (cli::performCliCommand (makeArgs ("export /nope/missing.audium"), context)
                     == cli::exitUsage);
        }
    }

    GIVEN ("the command table") {
        THEN ("all verbs are present with handlers and help text") {
            auto& commands = cli::getCliCommands();
            REQUIRE (commands.size() == 17);

            for (auto verb : { "info", "create", "import", "export", "analyze", "auto-edit", "assemble",
                               "split", "create-region", "set-region",
                               "remove-clip", "move-clip", "place-clip", "cleanup-regions",
                               "clip-gain", "clip-fades", "separate" }) {
                auto found = std::find_if (commands.begin(), commands.end(),
                                           [verb] (auto& spec) { return spec.verb == verb; });
                REQUIRE (found != commands.end());
                REQUIRE (found->run != nullptr);
                REQUIRE (found->usage.isNotEmpty());
                REQUIRE (found->shortHelp.isNotEmpty());
            }
        }
    }
}
