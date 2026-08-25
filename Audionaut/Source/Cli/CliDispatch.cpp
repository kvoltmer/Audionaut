//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Cli/CliDispatch.h"
#include "Cli/Commands/Commands.h"

namespace audium {
namespace cli {

const std::vector<CliCommandSpec>& getCliCommands()
{
    static const std::vector<CliCommandSpec> commands = {
        { "info",
          "info <project.audium> [--raw] [--json]",
          "Prints a summary of the project (tracks, clips, tempo).",
          "Prints a summary of the project. --raw dumps the full persistence JSON instead.",
          runInfo },

        { "create",
          "create <project.audium> [--channels N] [--json]",
          "Creates a new empty project with one N-channel track.",
          "Creates a new empty project (default: one stereo track) and saves it as a .audium package.",
          runCreate },

        { "import",
          "import <project.audium> <audio...> [--position SECONDS] [--json]",
          "Imports audio files into the project and saves it.",
          "Imports one or more audio files at the given position (default 0) and saves the project.",
          runImport },

        { "export",
          "export <project.audium> -o <out.wav> [--sample-rate N] [--bit-depth N]\n"
          "                          [--channels N] [--multi-mono] [--start S] [--length S] [--json]",
          "Renders the project offline to a WAV file.",
          "Renders the project offline (no audio device needed). --multi-mono writes one mono file per channel.",
          runExport },

        { "analyze",
          "analyze <project.audium|audio-file> [--types sbic,beat_degara] [--json]",
          "Runs audio analysis (Essentia) and caches the results.",
          "Analyses the project's audio files (or one given file) and persists the results next to the "
          "project so auto-edit/assemble and the GUI can reuse them. Exits 3 in builds without Essentia.",
          runAnalyze },

        { "auto-edit",
          "auto-edit <project.audium> [--track N] [--clip N] [--measures M]\n"
          "                          [--segments N] [--duration S] [--no-crossfades] [--json]",
          "Segments a clip using cached analysis results.",
          "Applies the auto-edit (segmentation) to a clip. Requires analysis results - run `analyze` first.",
          runAutoEdit },

        { "assemble",
          "assemble <project.audium> [--track N] [--duration S]\n"
          "                          [--mode random|sequential] [--seed N] [--json]",
          "Assembles a new arrangement from the project's regions.",
          "Builds an arrangement of the given duration from the project's regions, sequentially or at random.",
          runAssemble },
    };

    return commands;
}

int performCliCommand (const juce::ArgumentList& args, CliContext& context)
{
    if (args.size() == 0)
        return cliCommandNotPerformed;

    auto& first = args.arguments.getReference (0);
    if (first.isOption())
        return cliCommandNotPerformed;

    for (auto& spec : getCliCommands()) {
        if (first.text == spec.verb) {
            try {
                return spec.run (args, context);
            }
            catch (const std::exception& e) {
                return context.fail (exitFailure, "exception", e.what());
            }
            catch (...) {
                return context.fail (exitFailure, "exception", "unknown error");
            }
        }
    }

    return cliCommandNotPerformed;
}

} // namespace cli
} // namespace audium
