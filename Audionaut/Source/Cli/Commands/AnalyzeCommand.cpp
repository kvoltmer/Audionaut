//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Cli/Commands/Commands.h"
#include "Engine/ProjectFileStore.h"
#include "Cli/HeadlessEngineSession.h"

// Same auto-detection as the segmenters (see SBicSegmenter.cpp): the CMake
// builds define ESSENTIA_ENABLED globally, but the Projucer app build relies
// on per-file header detection - without this block the in-app CLI would
// report analysis unavailable even though the app links Essentia.
#ifndef ESSENTIA_ENABLED
 #if __has_include(<essentia/algorithmfactory.h>) && __has_include(<unsupported/Eigen/CXX11/Tensor>)
  #define ESSENTIA_ENABLED 1
 #else
  #define ESSENTIA_ENABLED 0
 #endif
#endif

#include "Engine/Analysis/AnalysisCache.h"
#include "Engine/Analysis/AnalysisProvider.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/Region/AudioRegion.h"
#include "Engine/Resource/AudioResource.h"

namespace audium {
namespace cli {

int runAnalyze (const juce::ArgumentList& args, CliContext& context)
{
#if ! ESSENTIA_ENABLED
    juce::ignoreUnused (args);
    return context.fail (exitUnavailable, "essentia_unavailable",
                         "this build was made without Essentia, so analysis is unavailable");
#else
    auto working = args;
    auto typesValue = takeOptionValue (working, "--types");

    auto types = typesValue.isNotEmpty()
                     ? analysisTypesFromString (typesValue.toStdString())
                     : AnalysisProvider::getMergeAnalysisTypes();
    if (types.empty())
        return context.fail (exitUsage, "usage", "--types matched no known analysis type");

    auto projectFile = resolveProjectFile (working);
    auto plain = getPlainArguments (working);
    if (projectFile == juce::File() && plain.isEmpty())
        return context.fail (exitUsage, "usage", "analyze requires a <project.audium> or an audio file");

    ScopedCoutToStderr guard (context.json);
    HeadlessEngineSession session;

    // The set of audio files to analyse: the project's, or the one given.
    std::vector<juce::File> audioFiles;

    if (projectFile != juce::File()) {
        std::string error;
        if (! session->getProjectFileStore()->open (projectFile, [&error] (std::string message) { error = message; }))
            return context.fail (exitFailure, "open_failed", error.empty() ? "failed to open project" : error);

        for (auto& track : session->getAudioTrackContainer()->getAudioTracks())
            for (auto& item : track->getPlayListContainer()->getPlayListItems())
                if (auto region = item->getRegion())
                    for (auto& resource : region->getAudioResources()) {
                        auto file = resource->getUrl().getLocalFile();
                        if (std::find (audioFiles.begin(), audioFiles.end(), file) == audioFiles.end())
                            audioFiles.push_back (file);
                    }
    }
    else {
        auto file = workingDirectory().getChildFile (plain[0]);
        if (! file.existsAsFile())
            return context.fail (exitUsage, "usage", "file not found: " + plain[0].toStdString());
        audioFiles.push_back (file);
    }

    if (audioFiles.empty())
        return context.fail (exitFailure, "no_audio", "the project references no audio files");

    auto provider = session->getAudioTrackContainer()->getAnalysisProvider();

    auto files = nlohmann::json::array();
    for (auto& file : audioFiles) {

        nlohmann::json analyses;
        for (auto type : types) {
            context.log ("analyzing " + file.getFileName() + " (" + analysisTypeToString (type) + ")");
            auto segments = provider->analyzeFile (file, type);

            nlohmann::json entry;
            entry["segments"] = segments;
            if (auto bpm = provider->getBpm (type, file); bpm > 0.0f)
                entry["bpm"] = bpm;

            analyses[analysisTypeToString (type)] = entry;
        }

        files.push_back ({ { "file", file.getFullPathName().toStdString() },
                           { "analyses", analyses } });
    }

    // Persist alongside the project so auto-edit/assemble (and the GUI) can
    // reuse the results without re-analysing.
    if (projectFile != juce::File())
        provider->getCache()->saveToFolder (projectFile.getParentDirectory());

    nlohmann::json result = { { "files", files } };
    if (context.json)
        return context.ok (result);

    std::cout << result.dump (2) << std::endl;
    return exitOk;
#endif
}

} // namespace cli
} // namespace audium
