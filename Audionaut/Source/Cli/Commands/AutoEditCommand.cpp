//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Cli/Commands/Commands.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Cli/CommandSession.h"

#include "Engine/AutoEdit/AutoEdit.h"

namespace audium {
namespace cli {

int runAutoEdit (const juce::ArgumentList& args, CliContext& context)
{
    auto working = args;
    AutoEditConfig config;
    config.trackId = takeOptionValue (working, "--track", "0").getIntValue();
    config.playlistItemId = takeOptionValue (working, "--clip", "-1").getIntValue();
    std::string optionError;
    double parsed = 0.0;
    // 0 measures is the documented "off" (segment by count instead)
    if (auto value = takeOptionValue (working, "--measures"); value.isNotEmpty()) {
        if (! parseNumericOption ("--measures", value, 0.0, false, parsed, optionError))
            return context.fail (exitUsage, "usage", optionError);
        config.segmentMeasures = parsed;
    }
    if (auto value = takeOptionValue (working, "--segments"); value.isNotEmpty()) {
        if (! parseNumericOption ("--segments", value, 1.0, false, parsed, optionError))
            return context.fail (exitUsage, "usage", optionError);
        config.numSegments = static_cast<int> (parsed);
    }
    if (auto value = takeOptionValue (working, "--duration"); value.isNotEmpty()) {
        if (! parseNumericOption ("--duration", value, 0.0, true, parsed, optionError))
            return context.fail (exitUsage, "usage", optionError);
        config.duration = parsed;
    }
    config.crossfades = ! working.removeOptionIfFound ("--no-crossfades");

    auto projectFile = resolveProjectFile (working);
    if (projectFile == juce::File())
        return context.fail (exitUsage, "usage", "auto-edit requires an existing <project.audium>");

    ScopedCoutToStderr guard (context.json);
    int openFailure = exitFailure;
    auto session = openProjectSession (projectFile, CommandAccess::mutating, context, openFailure);
    if (! session)
        return openFailure;

    std::string error;
    auto captureError = [&error] (std::string message) { error = message; };

    // Auto-edit consumes cached analysis results; run `analyze` first. Its
    // own error callback reports a missing/incomplete cache.
    AutoEdit autoEdit (session.get());
    if (! autoEdit.invokeAutoEdit (config, captureError))
        return context.fail (exitFailure, "auto_edit_failed",
                             error.empty() ? "auto-edit failed" : error);

    if (! session.commit (error))
        return context.fail (exitFailure, "save_failed", error.empty() ? "failed to save project" : error);

    context.log ("auto-edit applied");
    return context.ok ({ { "trackId", config.trackId },
                         { "numSegments", config.numSegments },
                         { "segmentMeasures", config.segmentMeasures },
                         { "crossfades", config.crossfades } });
}

int runAssemble (const juce::ArgumentList& args, CliContext& context)
{
    auto working = args;
    AssembleConfig config;
    config.crossfades = ! working.removeOptionIfFound ("--no-crossfades");
    config.trackId = takeOptionValue (working, "--track", "0").getIntValue();
    if (auto value = takeOptionValue (working, "--duration"); value.isNotEmpty()) {
        std::string optionError;
        if (! parseNumericOption ("--duration", value, 0.0, true, config.duration, optionError))
            return context.fail (exitUsage, "usage", optionError);
    }
    if (auto value = takeOptionValue (working, "--seed"); value.isNotEmpty())
        config.seed = static_cast<unsigned int> (value.getLargeIntValue());

    auto mode = takeOptionValue (working, "--mode", "sequential");
    if (mode == "random")
        config.mode = AssembleConfig::Mode::Random;
    else if (mode == "sequential")
        config.mode = AssembleConfig::Mode::Sequential;
    else
        return context.fail (exitUsage, "usage", "--mode must be random or sequential");

    auto projectFile = resolveProjectFile (working);
    if (projectFile == juce::File())
        return context.fail (exitUsage, "usage", "assemble requires an existing <project.audium>");

    ScopedCoutToStderr guard (context.json);
    int openFailure = exitFailure;
    auto session = openProjectSession (projectFile, CommandAccess::mutating, context, openFailure);
    if (! session)
        return openFailure;

    std::string error;
    auto captureError = [&error] (std::string message) { error = message; };

    AutoEdit autoEdit (session.get());
    if (! autoEdit.invokeAssemble (config, captureError))
        return context.fail (exitFailure, "assemble_failed",
                             error.empty() ? "assemble failed" : error);

    if (! session.commit (error))
        return context.fail (exitFailure, "save_failed", error.empty() ? "failed to save project" : error);

    context.log ("assemble applied");
    return context.ok ({ { "trackId", config.trackId },
                         { "durationSeconds", config.duration },
                         { "mode", mode.toStdString() },
                         { "crossfades", config.crossfades } });
}

} // namespace cli
} // namespace audium
