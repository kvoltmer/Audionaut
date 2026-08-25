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

    juce::ConsoleApplication app;
    app.addVersionCommand ("--version", "audionaut-cli 0.1.0");
    app.addHelpCommand ("--help|-h",
                        "audionaut-cli - headless access to .audium projects\n"
                        "Usage:",
                        true);

    // ConsoleApplication command callbacks return void; the per-command exit
    // code travels through this capture instead.
    auto exitCode = audium::cli::exitOk;

    for (auto& spec : audium::cli::getCliCommands())
        app.addCommand ({ spec.verb,
                          spec.usage,
                          spec.shortHelp,
                          spec.longHelp,
                          // the handler pointer is copied: a captured `&spec`
                          // is safe here (the table is a static) but reads
                          // like the classic dangling-loop-variable bug
                          [&exitCode, &context, run = spec.run] (const juce::ArgumentList& args) {
                              exitCode = run (args, context);
                          } });

    auto findResult = app.findAndRunCommand (argumentList);
    return findResult != 0 ? findResult : exitCode;
}
