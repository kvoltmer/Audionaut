//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Cli/Commands/Commands.h"
#include "Cli/HeadlessEngineSession.h"

#include "Engine/Analysis/AnalysisWorker.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Provider/TempoProvider.h"
#include "Engine/Resource/AudioResourceContainer.h"

namespace audium {
namespace cli {

int runImport (const juce::ArgumentList& args, CliContext& context)
{
    auto working = args;
    auto positionSeconds = takeOptionValue (working, "--position", "0").getDoubleValue();

    auto projectFile = resolveProjectFile (working);
    if (projectFile == juce::File())
        return context.fail (exitUsage, "usage", "import requires an existing <project.audium>");

    auto plain = getPlainArguments (working);
    juce::StringArray audioFiles;
    for (int i = 1; i < plain.size(); ++i) {
        auto file = juce::File::getCurrentWorkingDirectory().getChildFile (plain[i]);
        if (! file.existsAsFile())
            return context.fail (exitUsage, "usage", "audio file not found: " + plain[i].toStdString());
        audioFiles.add (file.getFullPathName());
    }
    if (audioFiles.isEmpty())
        return context.fail (exitUsage, "usage", "import requires at least one audio file");

    ScopedCoutToStderr guard (context.json);
    HeadlessEngineSession session;

    // Imports normally auto-enqueue background analysis; a run-to-completion
    // CLI wants deterministic output, so analysis stays an explicit `analyze`.
    session->getAudioResourceContainer()->getAnalysisWorker()->setAutoAnalysisEnabled (false);

    std::string error;
    auto captureError = [&error] (std::string message) { error = message; };

    if (! session->openFile (projectFile, captureError))
        return context.fail (exitFailure, "open_failed", error.empty() ? "failed to open project" : error);

    auto tempoProvider = session->getAudioTrackContainer()->getTempoProvider();
    auto positionClocks = tempoProvider->secondsToClocks (positionSeconds);

    if (! session->getAudioTrackContainer()->addAudioFiles (audioFiles, positionClocks, captureError, false))
        return context.fail (exitFailure, "import_failed", error.empty() ? "failed to import audio files" : error);

    if (! session->saveFile (projectFile, captureError))
        return context.fail (exitFailure, "save_failed", error.empty() ? "failed to save project" : error);

    context.log ("imported " + juce::String (audioFiles.size()) + " file(s)");
    return context.ok ({ { "importedFiles", audioFiles.size() },
                         { "positionSeconds", positionSeconds } });
}

} // namespace cli
} // namespace audium
