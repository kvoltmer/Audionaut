//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <memory>
#include <utility>
#include <vector>

#include <JuceHeader.h>

#include "Cli/CliContext.h"

namespace audium {

class AudioRegion;
class AudioTrack;
class AudioTrackContainer;
class TempoProvider;

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

/** `split <project> --at P [--unit bars|beats|seconds|clocks]` - split clips. */
int runSplit (const juce::ArgumentList& args, CliContext& context);

/** `create-region <project> --name N --start A --end B [--unit ...]` - named region. */
int runCreateRegion (const juce::ArgumentList& args, CliContext& context);

/** `set-region <project> --region N [--rename] [--start/--end/--length]` - edit a region. */
int runSetRegion (const juce::ArgumentList& args, CliContext& context);

/** `remove-clip <project> (--at P | --region N) [--track] [--delete-region]` */
int runRemoveClip (const juce::ArgumentList& args, CliContext& context);

/** `move-clip <project> (--at P | --region N) --to Q [--track]` */
int runMoveClip (const juce::ArgumentList& args, CliContext& context);

/** `place-clip <project> --region N --at P [--track]` */
int runPlaceClip (const juce::ArgumentList& args, CliContext& context);

/** `cleanup-regions <project>` - delete regions no clip uses. */
int runCleanupRegions (const juce::ArgumentList& args, CliContext& context);

/** `clip-gain <project> (--at P | --region N) --gain G [--db] [--channel C]` */
int runClipGain (const juce::ArgumentList& args, CliContext& context);

/** `clip-fades <project> (--at P | --region N) [--fade-in X ...]` */
int runClipFades (const juce::ArgumentList& args, CliContext& context);

/** `remove-track <project> --track N` - delete a whole track. */
int runRemoveTrack (const juce::ArgumentList& args, CliContext& context);

/** `remove-channel <project> --track N --channel C` - delete one channel. */
int runRemoveChannel (const juce::ArgumentList& args, CliContext& context);

/**
 * Musical time app-wide is 4/4 with 24 clocks per beat, i.e. 96 clocks per
 * bar (the arrangement grid makes the same assumption). As positions, bars
 * and beats are 1-based on the timeline; seconds and clocks are absolute
 * from 0. As durations (parseMusicalDuration), all units count from 0.
 */
constexpr double clocksPerBar  = 96.0;
constexpr double clocksPerBeat = 24.0;

bool parseMusicalPosition (const juce::String& value,
                           const juce::String& unit,
                           const TempoProvider& tempoProvider,
                           double& outClocks,
                           std::string& error);

bool parseMusicalDuration (const juce::String& value,
                           const juce::String& unit,
                           const TempoProvider& tempoProvider,
                           double& outClocks,
                           std::string& error);

/**
 * All (track, region) pairs whose region name matches exactly, optionally
 * filtered to one track id (trackId < 0 searches every track).
 */
std::vector<std::pair<std::shared_ptr<AudioTrack>, std::shared_ptr<AudioRegion>>>
findRegionsByName (const AudioTrackContainer& tracks, const juce::String& name, int trackId = -1);

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
