//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <functional>
#include <memory>
#include <string>
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

class ProjectSession;

/** The track with the id the verbs take (`--track N`), nullptr when there is none. */
std::shared_ptr<AudioTrack> findTrackById (const AudioTrackContainer& tracks, int trackId);

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

/** `export <project> -o <out.wav|.flac|.aiff|.ogg|.mp3> [format/range options]` - offline bounce. */
int runExport (const juce::ArgumentList& args, CliContext& context);

/** `analyze <project> [--types a,b]` - run the analyses, persist cache. */
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

/** `move-clip <project> (--at P | --region N) [--to Q] [--to-track N|new] [--track]` */
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

/** `track-pan <project> --track N --pan P [--channel C]` - pan a track's channel strips. */
int runTrackPan (const juce::ArgumentList& args, CliContext& context);
/** `add-insert <project> (--track N [--channel C | --all-channels] | --master) --type T [--at I]` - insert an effect. */
int runAddInsert (const juce::ArgumentList& args, CliContext& context);
/** `remove-insert <project> (--track N [--channel C] | --master) --index I` - remove an insert. */
int runRemoveInsert (const juce::ArgumentList& args, CliContext& context);
/** `set-insert <project> (--track N [--channel C] | --master) --index I [--param ID --value V] [--enabled on|off]` */
int runSetInsert (const juce::ArgumentList& args, CliContext& context);
/** `list-effects` - the effect types with their parameters. */
int runListEffects (const juce::ArgumentList& args, CliContext& context);

/** `add-aux <project> [--name N]` - a new aux bus. */
int runAddAux (const juce::ArgumentList& args, CliContext& context);
/** `remove-aux <project> --aux ID` - remove an aux bus and the sends into it. */
int runRemoveAux (const juce::ArgumentList& args, CliContext& context);
/** `set-aux <project> --aux ID [--name N] [--gain G] [--balance B] [--mute on|off] [--solo on|off]` */
int runSetAux (const juce::ArgumentList& args, CliContext& context);
/** `set-send <project> --track N [--channel C] --aux ID [--level L] [--pre-fader on|off] [--mute on|off]` */
int runSetSend (const juce::ArgumentList& args, CliContext& context);
/** `remove-send <project> --track N [--channel C] --aux ID` - remove a channel's send. */
int runRemoveSend (const juce::ArgumentList& args, CliContext& context);

/** `clip-speed <project> (--at P | --region N) (--ratio R | --semitones N | --length L)` - re-pitch a clip. */
int runClipSpeed (const juce::ArgumentList& args, CliContext& context);

/** `separate <project> [--track N --clip N --threads N --model PATH --no-mute-source]` - stems to new tracks. */
int runSeparate (const juce::ArgumentList& args, CliContext& context);

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
 * parseMusicalDuration without the sign check, for lengths that may be zero
 * or negative (a fade cleared, a ramp offset reaching outside the clip). Fails
 * only on an unknown unit.
 */
bool musicalDurationToClocks (const juce::String& value,
                              const juce::String& unit,
                              const TempoProvider& tempoProvider,
                              double& outClocks,
                              std::string& error);

/**
 * Validates a plain numeric option value (seconds, measures, counts - the
 * options that do not go through the musical-unit parsers above). Fails with
 * a message naming the option when the value is not a number or lies below
 * the bound: minimum itself is allowed unless exclusive is set.
 */
bool parseNumericOption (const juce::String& option,
                         const juce::String& value,
                         double minimum,
                         bool exclusive,
                         double& outValue,
                         std::string& error);

/**
 * All (track, region) pairs whose region name matches exactly, optionally
 * filtered to one track id (trackId < 0 searches every track).
 */
std::vector<std::pair<std::shared_ptr<AudioTrack>, std::shared_ptr<AudioRegion>>>
findRegionsByName (const AudioTrackContainer& tracks, const juce::String& name, int trackId = -1);

/** Resolves a project argument to its Project.json; invalid File() if absent. */
juce::File resolveProjectFile (const juce::ArgumentList& args, int argumentIndex = 0);

/** What an edit reports once its work is saved: a log line, then the result. */
struct EditOutcome
{
    juce::String log;
    nlohmann::json result;
};

/**
 * The lifecycle the editing verbs share: resolve the project argument (a usage
 * error naming `verb` when there is none), open it for a mutating session with
 * stdout redirected, run `edit`, then commit and report the outcome. `edit`
 * returns exitOk to have its work saved; any other code - a failure it has
 * already reported - is returned as is, and nothing is saved.
 *
 * The project is resolved from `args` as they stand at the call, so take the
 * value options out first (see getPlainArguments).
 */
int runProjectEdit (const juce::ArgumentList& args,
                    CliContext& context,
                    const std::string& verb,
                    const std::function<int (ProjectSession& session, EditOutcome& outcome)>& edit);

/**
 * @class ScopedWorkingDirectory
 * @brief Resolves relative paths against someone else's directory.
 *
 * A host runs verbs that were typed in another process, whose relative paths
 * mean nothing against its own working directory - the sandboxed app runs from
 * inside its container. While this stands, `workingDirectory()` answers with
 * the client's.
 */
class ScopedWorkingDirectory
{
public:
    explicit ScopedWorkingDirectory (const juce::File& directory);
    ~ScopedWorkingDirectory();

    ScopedWorkingDirectory (const ScopedWorkingDirectory&) = delete;
    ScopedWorkingDirectory& operator= (const ScopedWorkingDirectory&) = delete;
};

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
