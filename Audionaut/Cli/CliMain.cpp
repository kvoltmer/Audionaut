//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <JuceHeader.h>

#include "Cli/CliContext.h"
#include "Cli/CliDispatch.h"

//==============================================================================
// audionaut-cli - headless access to .audium projects for scripts and agents.
//
// The command set lives in Cli/CliDispatch.cpp (shared with the GUI app's
// in-app CLI mode); this file only provides the console entry point and the
// --help/--version UX.
//
// Global flags (any command):
//   --json    machine-readable result envelope on stdout, logs on stderr
//   --quiet   suppress log output
//
// Exit codes: 0 success, 1 operation failed, 2 usage error, 3 feature
// unavailable in this build.
//==============================================================================

int main (int argc, char* argv[])
{
    juce::ArgumentList argumentList (argc, argv);

    audium::cli::CliContext context;
    context.json = argumentList.removeOptionIfFound ("--json");
    context.quiet = argumentList.removeOptionIfFound ("--quiet");

    // The verbs dispatch directly (same path as the GUI's in-app CLI mode) -
    // NOT through ConsoleApplication::findAndRunCommand: its short-option
    // matching mistakes bare negative numbers for options (Argument::
    // isShortOption(char) stringifies the char through the int constructor,
    // so 'h' becomes "104" and any argument containing a '1', e.g.
    // "--gain -1", reroutes the whole command line to --help).
    const auto exitCode = audium::cli::performCliCommand (argumentList, context);
    if (exitCode != audium::cli::cliCommandNotPerformed)
        return exitCode;

    // No verb matched: ConsoleApplication only provides the --help/--version
    // UX (its matching quirk is harmless there).
    juce::ConsoleApplication app;
    app.addVersionCommand ("--version", "audionaut-cli 0.1.0");
    app.addHelpCommand ("--help|-h",
                        "audionaut-cli - headless access to .audium projects\n"
                        "Usage:",
                        true);

    // Registered for the --help listing only; the real dispatch already
    // declined this command line, so a match here (a verb behind an unknown
    // option) is not a runnable invocation.
    for (auto& spec : audium::cli::getCliCommands())
        app.addCommand ({ spec.verb, spec.usage, spec.shortHelp, spec.longHelp,
                          [] (const juce::ArgumentList&) {
                              juce::ConsoleApplication::fail ("Unrecognised arguments");
                          } });

    return app.findAndRunCommand (argumentList);
}
