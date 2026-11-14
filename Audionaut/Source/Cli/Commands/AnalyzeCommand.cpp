//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Cli/Commands/Commands.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Cli/CommandSession.h"

#include "Engine/Analysis/AnalysisAudioLoader.h"
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
    auto working = args;
    auto typesValue = takeOptionValue (working, "--types");

    // The names as the caller wrote them, each with the type it names: the
    // results are reported under those names, so a script asking for the
    // old "sbic" finds its result under "sbic". Unknown names are skipped,
    // as analysisTypesFromString does.
    std::vector<std::pair<std::string, AnalysisType>> requested;
    if (typesValue.isNotEmpty()) {
        for (auto token : juce::StringArray::fromTokens (typesValue, ",", {}))
            if (auto type = analysisTypeFromString (token.trim().toStdString()))
                requested.emplace_back (token.trim().toStdString(), *type);
    }
    else {
        for (auto type : AnalysisProvider::getMergeAnalysisTypes())
            requested.emplace_back (analysisTypeToString (type), type);
    }
    if (requested.empty())
        return context.fail (exitUsage, "usage", "--types matched no known analysis type");

    std::vector<AnalysisType> types;
    for (const auto& [name, type] : requested)
        if (std::find (types.begin(), types.end(), type) == types.end())
            types.push_back (type);

    auto projectFile = resolveProjectFile (working);
    auto plain = getPlainArguments (working);
    if (projectFile == juce::File() && plain.isEmpty())
        return context.fail (exitUsage, "usage", "analyze requires a <project.audium> or an audio file");

    ScopedCoutToStderr guard (context.json);

    // analyze is the one verb that also runs on a bare audio file, so the
    // session may carry no project at all.
    int openFailure = exitFailure;
    auto session = projectFile != juce::File()
                       ? openProjectSession (projectFile, CommandAccess::readOnly, context, openFailure)
                       : openEngineSession (context);
    if (! session)
        return openFailure;

    // The set of audio files to analyse: the project's, or the one given.
    std::vector<juce::File> audioFiles;

    if (projectFile != juce::File()) {
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

    // A file the decoder cannot open would come back as empty analyses that
    // look like results; say so instead.
    for (auto& file : audioFiles)
        if (auto problem = AnalysisAudioLoader::describeProblem (file); problem.isNotEmpty())
            return context.fail (exitFailure, "decode_failed",
                                 "cannot decode " + file.getFileName().toStdString() + ": " + problem.toStdString());

    auto provider = session->getAudioTrackContainer()->getAnalysisProvider();

    // One pass per file produces every requested type; the pass reports
    // its own progress, which is scaled into the file's share of the whole.
    ProgressSteps steps (context);
    const auto totalFiles = static_cast<double> (audioFiles.size());
    auto finishedFiles = 0;

    auto files = nlohmann::json::array();
    for (auto& file : audioFiles) {
        const auto message = "Analyzing " + file.getFileName();
        steps (finishedFiles / totalFiles, message);
        auto results = provider->analyzeFile (file, types, nullptr, [&] (double fraction)
        {
            steps ((finishedFiles + fraction) / totalFiles, message);
        });
        ++finishedFiles;

        nlohmann::json analyses;
        for (const auto& [name, type] : requested) {
            nlohmann::json entry;
            entry["segments"] = results.count (type) > 0 ? results.at (type) : std::vector<float>{};
            if (auto bpm = provider->getBpm (type, file); bpm > 0.0f)
                entry["bpm"] = bpm;
            analyses[name] = entry;
        }

        files.push_back ({ { "file", file.getFullPathName().toStdString() },
                           { "analyses", analyses } });
    }

    // Persist alongside the project so auto-edit/assemble (and the GUI) can
    // reuse the results without re-analysing.
    if (projectFile != juce::File())
        provider->getCache()->saveToFolder (projectFile.getParentDirectory());

    steps (1.0, "Analysis done");

    nlohmann::json result = { { "files", files } };
    if (context.json)
        return context.ok (result);

    std::cout << result.dump (2) << std::endl;
    return exitOk;
}

} // namespace cli
} // namespace audium
