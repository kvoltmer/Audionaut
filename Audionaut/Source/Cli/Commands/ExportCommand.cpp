//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Cli/Commands/Commands.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Cli/CommandSession.h"

#include "Engine/Export/AudioExporter.h"
#include "Engine/Export/ExportAudioConfig.h"
#include "Engine/Export/ExportFormat.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/Region/AudioRegion.h"

namespace audium {
namespace cli {

int runExport (const juce::ArgumentList& args, CliContext& context)
{
    auto working = args;

    auto outputValue = takeOptionValue (working, "--output|-o");
    auto sampleRateValue = takeOptionValue (working, "--sample-rate");
    auto bitDepthValue = takeOptionValue (working, "--bit-depth");
    auto bitRateValue = takeOptionValue (working, "--bitrate");
    auto startValue = takeOptionValue (working, "--start");
    auto lengthValue = takeOptionValue (working, "--length");
    auto channelsValue = takeOptionValue (working, "--channels");
    auto multiMono = working.removeOptionIfFound ("--multi-mono");
    auto regionName = takeOptionValue (working, "--region");
    auto trackId = takeOptionValue (working, "--track", "-1").getIntValue();

    if (regionName.isNotEmpty() && (startValue.isNotEmpty() || lengthValue.isNotEmpty()))
        return context.fail (exitUsage, "usage",
                             "--start/--length do not apply to a --region export (the region is its own range)");

    double startSeconds = 0.0, lengthSeconds = 0.0;
    std::string optionError;
    if (startValue.isNotEmpty()
        && ! parseNumericOption ("--start", startValue, 0.0, false, startSeconds, optionError))
        return context.fail (exitUsage, "usage", optionError);
    if (lengthValue.isNotEmpty()
        && ! parseNumericOption ("--length", lengthValue, 0.0, true, lengthSeconds, optionError))
        return context.fail (exitUsage, "usage", optionError);

    auto projectFile = resolveProjectFile (working);
    if (projectFile == juce::File())
        return context.fail (exitUsage, "usage", "export requires an existing <project.audium>");

    if (outputValue.isEmpty())
        return context.fail (exitUsage, "usage", "export requires -o <out.wav|out.flac|out.aiff|out.ogg|out.mp3>");

    // the format follows the extension
    auto outputFile = workingDirectory().getChildFile (outputValue);
    auto format = exportFormatForFile (outputFile);
    if (! format)
        return context.fail (exitUsage, "usage",
                             "the output file must end with " + exportExtensionList().toStdString());

    // a lossy format has a bit rate, a lossless one a bit depth
    auto rateList = [&] {
        juce::StringArray rates;
        for (auto kbps : bitRates (*format))
            rates.add (juce::String (kbps));
        return rates.joinIntoString (", ").toStdString();
    };
    if (isLossy (*format) && bitDepthValue.isNotEmpty())
        return context.fail (exitUsage, "usage",
                             "--bit-depth does not apply to " + formatName (*format).toStdString()
                                 + "; set its --bitrate (" + rateList() + " kbps) instead");
    if (! isLossy (*format) && bitRateValue.isNotEmpty())
        return context.fail (exitUsage, "usage",
                             "--bitrate applies to a lossy format (.ogg, .mp3); "
                                 + formatName (*format).toStdString() + " takes --bit-depth");

    auto quality = -1;
    if (bitRateValue.isNotEmpty()) {
        auto index = bitRateValue.containsOnly ("0123456789")
                   ? qualityIndexForBitRate (*format, bitRateValue.getIntValue())
                   : std::nullopt;
        if (! index)
            return context.fail (exitUsage, "usage",
                                 formatName (*format).toStdString() + " takes --bitrate "
                                     + rateList() + " (kbps)");
        quality = *index;
    }

    ScopedCoutToStderr guard (context.json);
    int openFailure = exitFailure;
    auto session = openProjectSession (projectFile, CommandAccess::isolated, context, openFailure);
    if (! session)
        return openFailure;

    std::string error;

    auto config = std::make_shared<ExportAudioConfig>();
    config->fileName = outputFile;
    config->format = *format;
    config->quality = quality;

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

        // A fresh item over the region, deliberately without any clip's
        // dynamics: a region always exports dry - gains and fades belong to
        // its timeline placements, the region is the raw material.
        config->playListItem = std::shared_ptr<PlayListItem> (
            new PlayListItem (*track->getPlayListContainer(), region, track->getSelectionManager()));
        config->numChannels = track->getNumAudioTrackChannels();
        config->sampleRate = region->getResourcesMaxSampleRate();
        // the source's depth, as deep as the format allows (a 32-bit float
        // source exported as FLAC becomes 24 bits); --bit-depth overrides it
        config->bitDepth = closestSupportedBitDepth (*format, region->getResourcesMaxBitDepth());
    }

    if (sampleRateValue.isNotEmpty())
        config->sampleRate = sampleRateValue.getDoubleValue();
    if (bitDepthValue.isNotEmpty())
        config->bitDepth = bitDepthValue.getIntValue();
    if (startValue.isNotEmpty())
        config->positionSeconds = startSeconds;
    if (lengthValue.isNotEmpty())
        config->lengthSeconds = lengthSeconds;

    config->multiMono = multiMono;
    if (channelsValue.isNotEmpty())
        config->numChannels = channelsValue.getIntValue();
    else if (config->multiMono && regionName.isEmpty())
        config->numChannels = session->getAudioTrackContainer()->getNumAudioTrackChannels();

    if (config->sampleRate <= 0 || config->bitDepth <= 0 || config->numChannels < 1)
        return context.fail (exitUsage, "usage", "invalid export format options");

    ProgressSteps steps (context);
    AudioExporter exporter (*session.get(), config);
    auto written = exporter.bounce ([&steps] (double progress) {
        steps (progress, "Exporting");
        return true;
    });

    if (! written) {
        using Failure = ExportAudioConfig::Failure;
        auto error = config->error.toStdString();

        if (config->failure == Failure::unsupportedFormat)
            return context.fail (exitUsage, "usage", error);

        // Running inside the app means running inside its sandbox, which can
        // only write to the Music folder and to places the user picked. A
        // path outside those cannot even be opened.
        if (config->failure == Failure::cannotOpenOutput && session.isHosted())
            return context.fail (exitFailure, "sandbox_denied",
                                 "Audionaut is running the export and could not write to \""
                                     + outputFile.getFullPathName().toStdString()
                                     + "\". Choose a location inside your Music folder, or quit "
                                       "Audionaut to export with the command line instead.");

        return context.fail (exitFailure, "export_failed", error);
    }

    // the bounce's last report lands short of 1 (it reports before each block)
    steps (1.0, "Exporting");

    // multi-mono writes -01.wav, -02.wav, ... (in the export's format) instead of the base file
    auto produced = config->multiMono ? AudioExporter::monoFileFor (outputFile, 1, *format) : outputFile;

    context.log ("exported " + produced.getFullPathName());
    nlohmann::json result = { { "outputFile", produced.getFullPathName().toStdString() },
                              { "format", fileExtension (*format).substring (1).toStdString() },
                              { "sampleRate", config->sampleRate },
                              { "numChannels", config->numChannels },
                              { "multiMono", config->multiMono } };
    if (isLossy (*format))
        result["bitrateKbps"] = bitRates (*format)[quality >= 0 ? quality : defaultQuality (*format)];
    else
        result["bitDepth"] = config->bitDepth;
    if (regionName.isNotEmpty())
        result["region"] = regionName.toStdString();
    return context.ok (result);
}

} // namespace cli
} // namespace audium
