//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Cli/Commands/Commands.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Cli/HeadlessEngineSession.h"

#include "Engine/Export/AudioExporter.h"
#include "Engine/Export/ExportAudioConfig.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/PlayList/ClipDynamics.h"
#include "Engine/Region/AudioRegion.h"

namespace audium {
namespace cli {

int runExport (const juce::ArgumentList& args, CliContext& context)
{
    auto working = args;

    auto outputValue = takeOptionValue (working, "--output|-o");
    auto sampleRateValue = takeOptionValue (working, "--sample-rate");
    auto bitDepthValue = takeOptionValue (working, "--bit-depth");
    auto startValue = takeOptionValue (working, "--start");
    auto lengthValue = takeOptionValue (working, "--length");
    auto channelsValue = takeOptionValue (working, "--channels");
    auto multiMono = working.removeOptionIfFound ("--multi-mono");
    auto regionName = takeOptionValue (working, "--region");
    auto trackId = takeOptionValue (working, "--track", "-1").getIntValue();

    if (regionName.isNotEmpty() && (startValue.isNotEmpty() || lengthValue.isNotEmpty()))
        return context.fail (exitUsage, "usage",
                             "--start/--length do not apply to a --region export (the region is its own range)");

    auto projectFile = resolveProjectFile (working);
    if (projectFile == juce::File())
        return context.fail (exitUsage, "usage", "export requires an existing <project.audium>");

    if (outputValue.isEmpty())
        return context.fail (exitUsage, "usage", "export requires -o <out.wav>");

    auto outputFile = workingDirectory().getChildFile (outputValue);
    if (! outputFile.hasFileExtension (".wav"))
        return context.fail (exitUsage, "usage", "the output file must end with .wav");

    ScopedCoutToStderr guard (context.json);
    HeadlessEngineSession session;

    std::string error;
    if (! session->getProjectFileStore()->open (projectFile, [&error] (std::string message) { error = message; }))
        return context.fail (exitFailure, "open_failed", error.empty() ? "failed to open project" : error);

    auto config = std::make_shared<ExportAudioConfig>();
    config->fileName = outputFile;

    if (regionName.isNotEmpty()) {
        auto matches = findRegionsByName (*session->getAudioTrackContainer(), regionName, trackId);
        if (matches.empty())
            return context.fail (exitFailure, "region_not_found",
                                 "no region named \"" + regionName.toStdString() + "\"");
        if (matches.size() > 1)
            return context.fail (exitFailure, "ambiguous_region",
                                 "multiple regions named \"" + regionName.toStdString()
                                     + "\"; pass --track to disambiguate");

        auto track = matches.front().first;
        auto region = matches.front().second;

        // The same recipe as the GUI's per-clip export (PlayListItemExport):
        // a fresh item over the region, sounding like its timeline placement.
        auto exportItem = std::shared_ptr<PlayListItem> (
            new PlayListItem (*track->getPlayListContainer(), region, track->getSelectionManager()));

        // Carry the placement's gains, fades and fade extensions over. An
        // unplaced region exports dry; a region placed more than once is
        // ambiguous about which clip's dynamics to use.
        std::vector<PlayListItem*> placements;
        for (auto& item : track->getPlayListContainer()->getPlayListItems())
            if (item->getRegion() == region)
                placements.push_back (item.get());
        if (placements.size() > 1)
            return context.fail (exitFailure, "ambiguous_clip",
                                 "the region is placed more than once; export applies one clip's "
                                 "gains and fades, remove the extra placements first");
        if (placements.size() == 1)
            exportItem->getDynamics().copyFrom (placements.front()->getDynamics());

        config->playListItem = exportItem;
        config->numChannels = track->getNumAudioTrackChannels();
        config->sampleRate = region->getResourcesMaxSampleRate();
        config->bitDepth = region->getResourcesMaxBitDepth();
    }

    if (sampleRateValue.isNotEmpty())
        config->sampleRate = sampleRateValue.getDoubleValue();
    if (bitDepthValue.isNotEmpty())
        config->bitDepth = bitDepthValue.getIntValue();
    if (startValue.isNotEmpty())
        config->positionSeconds = startValue.getDoubleValue();
    if (lengthValue.isNotEmpty())
        config->lengthSeconds = lengthValue.getDoubleValue();

    config->multiMono = multiMono;
    if (channelsValue.isNotEmpty())
        config->numChannels = channelsValue.getIntValue();
    else if (config->multiMono && regionName.isEmpty())
        config->numChannels = session->getAudioTrackContainer()->getNumAudioTrackChannels();

    if (config->sampleRate <= 0 || config->bitDepth <= 0 || config->numChannels < 1)
        return context.fail (exitUsage, "usage", "invalid export format options");

    auto lastLoggedProgress = 0.0;
    AudioExporter exporter (*session.get(), config);
    exporter.bounce ([&] (double progress) {
        if (progress - lastLoggedProgress >= 0.1) {
            lastLoggedProgress = progress;
            context.log ("export: " + juce::String (juce::roundToInt (progress * 100)) + "%");
        }
        return true;
    });

    // multi-mono splits into -01.wav, -02.wav, ... and deletes the base file
    auto firstMonoFile = outputFile.getSiblingFile (outputFile.getFileNameWithoutExtension() + "-01.wav");
    auto produced = config->multiMono ? firstMonoFile : outputFile;
    if (! produced.existsAsFile())
        return context.fail (exitFailure, "export_failed", "export produced no output file");

    context.log ("exported " + produced.getFullPathName());
    nlohmann::json result = { { "outputFile", produced.getFullPathName().toStdString() },
                              { "sampleRate", config->sampleRate },
                              { "bitDepth", config->bitDepth },
                              { "numChannels", config->numChannels },
                              { "multiMono", config->multiMono } };
    if (regionName.isNotEmpty())
        result["region"] = regionName.toStdString();
    return context.ok (result);
}

} // namespace cli
} // namespace audium
