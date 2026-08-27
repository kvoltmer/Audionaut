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
    else if (config->multiMono)
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
    return context.ok ({ { "outputFile", produced.getFullPathName().toStdString() },
                         { "sampleRate", config->sampleRate },
                         { "bitDepth", config->bitDepth },
                         { "numChannels", config->numChannels },
                         { "multiMono", config->multiMono } });
}

} // namespace cli
} // namespace audium
