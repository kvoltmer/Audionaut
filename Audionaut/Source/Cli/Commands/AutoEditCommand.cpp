//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Cli/Commands/Commands.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Cli/HeadlessEngineSession.h"

#include "Engine/AutoEdit/AutoEdit.h"

namespace audium {
namespace cli {

int runAutoEdit (const juce::ArgumentList& args, CliContext& context)
{
    auto working = args;
    AutoEditConfig config;
    config.trackId = takeOptionValue (working, "--track", "0").getIntValue();
    config.playlistItemId = takeOptionValue (working, "--clip", "-1").getIntValue();
    if (auto value = takeOptionValue (working, "--measures"); value.isNotEmpty())
        config.segmentMeasures = value.getDoubleValue();
    if (auto value = takeOptionValue (working, "--segments"); value.isNotEmpty())
        config.numSegments = value.getIntValue();
    if (auto value = takeOptionValue (working, "--duration"); value.isNotEmpty())
        config.duration = value.getDoubleValue();
    config.crossfades = ! working.removeOptionIfFound ("--no-crossfades");

    auto projectFile = resolveProjectFile (working);
    if (projectFile == juce::File())
        return context.fail (exitUsage, "usage", "auto-edit requires an existing <project.audium>");

    ScopedCoutToStderr guard (context.json);
    HeadlessEngineSession session;

    std::string error;
    auto captureError = [&error] (std::string message) { error = message; };

    if (! session->getProjectFileStore()->open (projectFile, captureError))
        return context.fail (exitFailure, "open_failed", error.empty() ? "failed to open project" : error);

    // Auto-edit consumes cached analysis results; run `analyze` first. Its
    // own error callback reports a missing/incomplete cache.
    AutoEdit autoEdit (session.get());
    if (! autoEdit.invokeAutoEdit (config, captureError))
        return context.fail (exitFailure, "auto_edit_failed",
                             error.empty() ? "auto-edit failed" : error);

    if (! session->getProjectFileStore()->save (projectFile, captureError))
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
    config.trackId = takeOptionValue (working, "--track", "0").getIntValue();
    if (auto value = takeOptionValue (working, "--duration"); value.isNotEmpty())
        config.duration = value.getDoubleValue();
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
    HeadlessEngineSession session;

    std::string error;
    auto captureError = [&error] (std::string message) { error = message; };

    if (! session->getProjectFileStore()->open (projectFile, captureError))
        return context.fail (exitFailure, "open_failed", error.empty() ? "failed to open project" : error);

    AutoEdit autoEdit (session.get());
    if (! autoEdit.invokeAssemble (config, captureError))
        return context.fail (exitFailure, "assemble_failed",
                             error.empty() ? "assemble failed" : error);

    if (! session->getProjectFileStore()->save (projectFile, captureError))
        return context.fail (exitFailure, "save_failed", error.empty() ? "failed to save project" : error);

    context.log ("assemble applied");
    return context.ok ({ { "trackId", config.trackId },
                         { "durationSeconds", config.duration },
                         { "mode", mode.toStdString() } });
}

} // namespace cli
} // namespace audium
