//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <JuceHeader.h>

#include "Cli/CliContext.h"
#include "Cli/Commands/Commands.h"

//==============================================================================
// audionaut-cli - headless access to .audium projects for scripts and agents.
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

    app.addCommand ({ "info",
                      "info <project.audium> [--raw] [--json]",
                      "Prints a summary of the project (tracks, clips, tempo).",
                      "Prints a summary of the project. --raw dumps the full persistence JSON instead.",
                      [&] (const juce::ArgumentList& args) { exitCode = audium::cli::runInfo (args, context); } });

    app.addCommand ({ "create",
                      "create <project.audium> [--channels N] [--json]",
                      "Creates a new empty project with one N-channel track.",
                      "Creates a new empty project (default: one stereo track) and saves it as a .audium package.",
                      [&] (const juce::ArgumentList& args) { exitCode = audium::cli::runCreate (args, context); } });

    app.addCommand ({ "import",
                      "import <project.audium> <audio...> [--position SECONDS] [--json]",
                      "Imports audio files into the project and saves it.",
                      "Imports one or more audio files at the given position (default 0) and saves the project.",
                      [&] (const juce::ArgumentList& args) { exitCode = audium::cli::runImport (args, context); } });

    app.addCommand ({ "export",
                      "export <project.audium> -o <out.wav> [--sample-rate N] [--bit-depth N]\n"
                      "                          [--channels N] [--multi-mono] [--start S] [--length S] [--json]",
                      "Renders the project offline to a WAV file.",
                      "Renders the project offline (no audio device needed). --multi-mono writes one mono file per channel.",
                      [&] (const juce::ArgumentList& args) { exitCode = audium::cli::runExport (args, context); } });

    app.addCommand ({ "analyze",
                      "analyze <project.audium|audio-file> [--types sbic,beat_degara] [--json]",
                      "Runs audio analysis (Essentia) and caches the results.",
                      "Analyses the project's audio files (or one given file) and persists the results next to the "
                      "project so auto-edit/assemble and the GUI can reuse them. Exits 3 in builds without Essentia.",
                      [&] (const juce::ArgumentList& args) { exitCode = audium::cli::runAnalyze (args, context); } });

    app.addCommand ({ "auto-edit",
                      "auto-edit <project.audium> [--track N] [--clip N] [--measures M]\n"
                      "                          [--segments N] [--duration S] [--no-crossfades] [--json]",
                      "Segments a clip using cached analysis results.",
                      "Applies the auto-edit (segmentation) to a clip. Requires analysis results - run `analyze` first.",
                      [&] (const juce::ArgumentList& args) { exitCode = audium::cli::runAutoEdit (args, context); } });

    app.addCommand ({ "assemble",
                      "assemble <project.audium> [--track N] [--duration S]\n"
                      "                          [--mode random|sequential] [--seed N] [--json]",
                      "Assembles a new arrangement from the project's regions.",
                      "Builds an arrangement of the given duration from the project's regions, sequentially or at random.",
                      [&] (const juce::ArgumentList& args) { exitCode = audium::cli::runAssemble (args, context); } });

    auto findResult = app.findAndRunCommand (argumentList);
    return findResult != 0 ? findResult : exitCode;
}
