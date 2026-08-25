//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Cli/CliContext.h"

namespace audium {
namespace cli {

/**
 * The CLI's verb set. Each command runs to completion inside its own
 * HeadlessEngineSession and returns one of the exit codes in CliContext.h.
 * The project argument is the first plain (non-option) argument; commands
 * accept either the .audium package directory or the Project.json inside it.
 */

/** `info <project> [--raw]` - project summary (or full persistence JSON). */
int runInfo (const juce::ArgumentList& args, CliContext& context);

/** `create <project> [--channels N]` - new empty project. */
int runCreate (const juce::ArgumentList& args, CliContext& context);

/** `import <project> <audio...> [--position SECONDS]` - add audio files. */
int runImport (const juce::ArgumentList& args, CliContext& context);

/** `export <project> -o <out.wav> [format/range options]` - offline bounce. */
int runExport (const juce::ArgumentList& args, CliContext& context);

/** `analyze <project> [--types a,b]` - run Essentia analyses, persist cache. */
int runAnalyze (const juce::ArgumentList& args, CliContext& context);

/** `auto-edit <project> [--track N --clip N --measures M]` - segment a clip. */
int runAutoEdit (const juce::ArgumentList& args, CliContext& context);

/** `assemble <project> [--duration S --mode random|sequential]` - build a cut. */
int runAssemble (const juce::ArgumentList& args, CliContext& context);

/** Resolves a project argument to its Project.json; invalid File() if absent. */
juce::File resolveProjectFile (const juce::ArgumentList& args, int argumentIndex = 0);

/**
 * The plain (non-option) arguments after the command word, in order.
 * `info foo.audium --json` yields { "foo.audium" }.
 *
 * Take value options out first (takeOptionValue) or their values are
 * misread as plain arguments.
 */
juce::StringArray getPlainArguments (const juce::ArgumentList& args);

/**
 * Consumes an option and its value from the list, accepting both
 * `--opt=value` and `--opt value` (JUCE's own getValueForOption only
 * understands the `=` form for long options). Returns defaultValue when the
 * option is absent, and an empty string when it is present without a value.
 */
juce::String takeOptionValue (juce::ArgumentList& args,
                              juce::StringRef option,
                              const juce::String& defaultValue = {});

/**
 * The directory relative CLI paths resolve against. Prefers the shell's
 * $PWD over getCurrentWorkingDirectory(): the sandboxed GUI app is chdir'd
 * into its container during process init, so in in-app CLI mode the real
 * invocation directory only survives in the environment.
 */
juce::File workingDirectory();

} // namespace cli
} // namespace audium
