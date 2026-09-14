//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Cli/Commands/Commands.h"

#include <chrono>

#include "Engine/AudioSources/StretchAudioSource.h"
#include "Engine/AudioSources/Stretch/StretchBackend.h"
#include "Engine/PlayList/ClipSpeed.h"

namespace audium {
namespace cli {

namespace {

struct RestoreSelectedEngine
{
    StretchEngine previous = StretchEngines::getSelected();
    ~RestoreSelectedEngine() { StretchEngines::setSelected (previous); }
};

double toDecibels (float gain)
{
    return gain > 0.0f ? 20.0 * std::log10 (static_cast<double> (gain)) : -120.0;
}

} // namespace

/**
 * stretch-eval: renders one audio file through the production stretch node
 * with every requested engine at every requested ratio, writes the results
 * as WAV files for listening, and reports render cost and levels. The
 * offline half of the engine evaluation; the Settings dialog is the
 * in-app half.
 */
int runStretchEval (const juce::ArgumentList& args, CliContext& context)
{
    auto working = args;
    const auto ratiosValue  = takeOptionValue (working, "--ratios", "0.5,0.75,1.25,2");
    const auto enginesValue = takeOptionValue (working, "--engines", "all");
    const auto outValue     = takeOptionValue (working, "--out");
    const auto blockValue   = takeOptionValue (working, "--block", "512");

    const auto plain = getPlainArguments (working);
    if (plain.isEmpty())
        return context.fail (exitUsage, "usage", "stretch-eval requires an <audio-file>");

    const auto inputFile = juce::File::getCurrentWorkingDirectory().getChildFile (plain[0]);
    if (! inputFile.existsAsFile())
        return context.fail (exitUsage, "usage", "no such audio file: " + inputFile.getFullPathName().toStdString());

    // engines
    std::vector<StretchEngine> engines;
    if (enginesValue == "all")
        engines = StretchEngines::available();
    else
        for (const auto& name : juce::StringArray::fromTokens (enginesValue, ",", ""))
        {
            const auto engine = StretchEngines::fromName (name.trim().toStdString());
            if (! engine.has_value())
                return context.fail (exitUsage, "usage", "unknown engine: " + name.toStdString());
            if (! StretchEngines::isAvailable (*engine))
                return context.fail (exitUnavailable, "engine_unavailable",
                                     "this build was made without " + name.toStdString());
            engines.push_back (*engine);
        }

    // ratios
    std::vector<double> ratios;
    for (const auto& token : juce::StringArray::fromTokens (ratiosValue, ",", ""))
    {
        const auto ratio = token.trim().getDoubleValue();
        if (ratio < ClipSpeed::minSpeedRatio || ratio > ClipSpeed::maxSpeedRatio)
            return context.fail (exitUsage, "usage",
                                 "ratios must be between " + juce::String (ClipSpeed::minSpeedRatio).toStdString()
                                 + " and " + juce::String (ClipSpeed::maxSpeedRatio).toStdString());
        ratios.push_back (ratio);
    }
    if (ratios.empty())
        return context.fail (exitUsage, "usage", "--ratios must name at least one ratio");

    const auto blockSize = juce::jlimit (32, 8192, blockValue.getIntValue());

    const auto outDir = outValue.isNotEmpty() ? juce::File::getCurrentWorkingDirectory().getChildFile (outValue)
                                              : inputFile.getParentDirectory();
    if (! outDir.createDirectory())
        return context.fail (exitFailure, "output_failed", "cannot create " + outDir.getFullPathName().toStdString());

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (inputFile));
    if (reader == nullptr)
        return context.fail (exitFailure, "read_failed", "cannot read " + inputFile.getFullPathName().toStdString());

    const auto numChannels = static_cast<int> (reader->numChannels);
    const auto sampleRate = reader->sampleRate;
    const auto inputSeconds = static_cast<double> (reader->lengthInSamples) / sampleRate;

    RestoreSelectedEngine restore;
    nlohmann::json engineResults = nlohmann::json::array();

    for (auto engine : engines)
    {
        StretchEngines::setSelected (engine);
        nlohmann::json ratioResults = nlohmann::json::array();

        for (auto ratio : ratios)
        {
            juce::AudioFormatReaderSource source (reader.get(), false);
            StretchAudioSource node (&source, numChannels);

            node.prepareToPlay (blockSize, sampleRate);
            node.setEnabled (true);
            node.setSpeedRatio (ratio);
            source.setNextReadPosition (0);
            node.flushBuffers();

            const auto outputSamples = static_cast<int> (std::ceil (static_cast<double> (reader->lengthInSamples) / ratio));
            juce::AudioBuffer<float> rendered (numChannels, outputSamples);
            rendered.clear();

            const auto started = std::chrono::steady_clock::now();
            for (int done = 0; done < outputSamples;)
            {
                const auto chunk = juce::jmin (blockSize, outputSamples - done);
                juce::AudioSourceChannelInfo info (&rendered, done, chunk);
                node.getNextAudioBlock (info);
                done += chunk;
            }
            const auto renderSeconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - started).count();
            node.releaseResources();

            // the file: <stem>-<engine>-x<ratio>.wav
            const auto outFile = outDir.getChildFile (inputFile.getFileNameWithoutExtension()
                                                      + "-" + StretchEngines::name (engine)
                                                      + "-x" + juce::String (ratio, 2) + ".wav");
            {
                juce::WavAudioFormat wav;
                std::unique_ptr<juce::FileOutputStream> stream (outFile.createOutputStream());
                if (stream == nullptr)
                    return context.fail (exitFailure, "output_failed", "cannot write " + outFile.getFullPathName().toStdString());
                stream->setPosition (0);
                stream->truncate();

                std::unique_ptr<juce::AudioFormatWriter> writer (
                    wav.createWriterFor (stream.get(), sampleRate, static_cast<unsigned int> (numChannels), 24, {}, 0));
                if (writer == nullptr)
                    return context.fail (exitFailure, "output_failed", "cannot encode " + outFile.getFullPathName().toStdString());
                stream.release();   // the writer owns it now
                writer->writeFromAudioSampleBuffer (rendered, 0, outputSamples);
            }

            const auto outputSeconds = outputSamples / sampleRate;
            ratioResults.push_back ({
                { "ratio", ratio },
                { "outputSeconds", outputSeconds },
                { "renderSeconds", renderSeconds },
                { "realtimeFactor", renderSeconds > 0.0 ? outputSeconds / renderSeconds : 0.0 },
                { "peakDb", toDecibels (rendered.getMagnitude (0, outputSamples)) },
                { "rmsDb", toDecibels (rendered.getRMSLevel (0, 0, outputSamples)) },
                { "file", outFile.getFullPathName().toStdString() } });

            context.log (juce::String (StretchEngines::name (engine)) + " x" + juce::String (ratio, 2)
                         + ": " + juce::String (outputSeconds, 2) + " s in " + juce::String (renderSeconds, 3)
                         + " s -> " + outFile.getFileName());
        }

        engineResults.push_back ({ { "engine", StretchEngines::name (engine) },
                                   { "licence", StretchEngines::licence (engine) },
                                   { "ratios", ratioResults } });
    }

    return context.ok ({ { "file", inputFile.getFullPathName().toStdString() },
                         { "sampleRate", sampleRate },
                         { "channels", numChannels },
                         { "inputSeconds", inputSeconds },
                         { "blockSize", blockSize },
                         { "engines", engineResults } });
}

} // namespace cli
} // namespace audium
