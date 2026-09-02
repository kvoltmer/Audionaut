//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Cli/Commands/Commands.h"
#include "Cli/HeadlessEngineSession.h"
#include "Engine/Project/ProjectFileStore.h"

#include "Engine/Separation/DemucsBackend.h"
#include "Engine/Separation/DemucsModelStore.h"
#include "Engine/Separation/FakeSeparationBackend.h"
#include "Engine/Separation/StemSeparator.h"

namespace audium {
namespace cli {

int runSeparate (const juce::ArgumentList& args, CliContext& context)
{
    auto working = args;

    SeparationConfig config;
    config.trackId = takeOptionValue (working, "--track", "0").getIntValue();
    config.playlistItemId = takeOptionValue (working, "--clip", "-1").getIntValue();
    config.numThreads = takeOptionValue (working, "--threads",
                                         juce::String (juce::jmax (1, juce::SystemStats::getNumPhysicalCpus())))
                            .getIntValue();

    config.muteSourceTrack = ! working.removeOptionIfFound ("--no-mute-source");

    const auto modelPath = takeOptionValue (working, "--model");
    const auto backendName = takeOptionValue (working, "--backend", "demucs");

    if (backendName != "demucs" && backendName != "fake")
        return context.fail (exitUsage, "usage", "--backend must be demucs or fake");

    if (config.numThreads < 1)
        return context.fail (exitUsage, "usage", "--threads must be at least 1");

    auto projectFile = resolveProjectFile (working);
    if (projectFile == juce::File())
        return context.fail (exitUsage, "usage", "separate requires an existing <project.audium>");

    // The backend is picked before the engine comes up so a missing model
    // fails fast, without opening the project.
    std::shared_ptr<SeparationBackend> backend;

    if (backendName == "fake")
    {
        backend = std::make_shared<FakeSeparationBackend>();
    }
    else
    {
        if (! DemucsBackend::isCompiledIn())
            return context.fail (exitUnavailable, "demucs_unavailable",
                                 "this build was made without Demucs stem separation");

        const auto modelFile = modelPath.isNotEmpty() ? workingDirectory().getChildFile (modelPath)
                                                      : DemucsModelStore::createDefault().getModelFile();

        if (! modelFile.existsAsFile())
            return context.fail (exitFailure, "model_missing",
                                 "the Demucs model is not installed (expected at "
                                 + modelFile.getFullPathName().toStdString()
                                 + "); download it from the app's Settings > Separation, or pass --model");

        backend = std::make_shared<DemucsBackend> (modelFile, config.numThreads);
    }

    ScopedCoutToStderr guard (context.json);
    HeadlessEngineSession session;

    std::string error;
    auto captureError = [&error] (std::string message) { error = message; };

    if (! session->getProjectFileStore()->open (projectFile, captureError))
        return context.fail (exitFailure, "open_failed", error.empty() ? "failed to open project" : error);

    StemSeparator separator (session.get(), backend);

    // Progress goes to stderr in coarse steps: the real backend reports a
    // few times a second for minutes.
    auto lastReported = -1;
    auto progress = [&context, &lastReported] (double fraction, const juce::String& message)
    {
        const auto percent = static_cast<int> (fraction * 100.0);

        if (percent / 5 != lastReported / 5 || percent == 100)
        {
            lastReported = percent;
            context.log (message + " " + juce::String (percent) + "%");
        }

        return true;
    };

    std::vector<int> newTrackIds;
    juce::String separationError;

    if (! separator.separate (config, progress, newTrackIds, separationError))
        return context.fail (exitFailure, "separation_failed",
                             separationError.isEmpty() ? "separation failed" : separationError.toStdString());

    if (! session->getProjectFileStore()->save (projectFile, captureError))
        return context.fail (exitFailure, "save_failed", error.empty() ? "failed to save project" : error);

    context.log ("stems added");

    nlohmann::json stems = nlohmann::json::array();

    for (auto index = 0; index < numStems && index < static_cast<int> (newTrackIds.size()); ++index)
    {
        const auto stem = stemFromIndex (index);
        auto track = session->getAudioTrackContainer()->getAudioTrack (newTrackIds[static_cast<size_t> (index)]);

        stems.push_back ({ { "stem", stemToString (stem) },
                           { "trackId", newTrackIds[static_cast<size_t> (index)] },
                           { "trackName", track != nullptr ? track->getAudioTrackName().toStdString() : std::string() } });
    }

    return context.ok ({ { "sourceTrackId", config.trackId },
                         { "sourceClipId", config.playlistItemId < 0 ? 0 : config.playlistItemId },
                         { "sourceTrackMuted", config.muteSourceTrack },
                         { "backend", backendName.toStdString() },
                         { "stems", stems } });
}

} // namespace cli
} // namespace audium
