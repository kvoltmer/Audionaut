//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Engine/AudiumEngine.h"
#include "Engine/Export/ExportFormat.h"
#include "Engine/AudioSources/RenderTiming.h"
#include "Engine/Link/LinkAudioDevice.h"
#include "Engine/PlayList/PlayListScheduler.h"

namespace audium {

/**
 * @class AudioExporter
 * @brief Exports (bounces) a project or single playlist item to an audio file.
 *
 * UI-agnostic: progress reporting and cancellation are delegated to an
 * optional callback, so the export can run headless (tests, CLI). The GUI
 * progress window lives in AudioExportThread, which wraps this class.
 */
class AudioExporter
{
public:
    AudioExporter(AudiumEngine &audiumEngine_,
                  std::shared_ptr<ExportAudioConfig> config_) :
        audiumEngine(audiumEngine_),
        config(config_)
    {
    }

    /**
     * @brief Exports audio to a file based on the provided configuration.
     * @param shouldContinue Called with the current progress (0..1); return
     *        false to cancel the export. Pass nothing to run to completion.
     * @return True when the output file (for multi-mono: every mono file)
     *         was written. False after a cancel (config->userCanceled) or a
     *         failure, which config->failure and config->error describe;
     *         neither leaves a partial file behind.
     */
    bool bounce(const std::function<bool(double)> &shouldContinue = {})
    {
        config->failure = ExportAudioConfig::Failure::none;
        config->error.clear();

        auto updateProgress = [this, &shouldContinue]() {
            if (shouldContinue != nullptr && !shouldContinue(config->progress))
                config->userCanceled = true;
        };

        // offline: the voices wait for their read-ahead instead of playing
        // silence, so clip starts (and the stretchers' priming) stay exact
        struct OfflineScope {
            OfflineScope()  { RenderTiming::setOffline (true); }
            ~OfflineScope() { RenderTiming::setOffline (false); }
        } offlineScope;

        // and flagged: an external reload or a hosted agent edit waits for
        // the render instead of rebuilding the graph underneath it
        const PlayListScheduler::ScopedOfflineRender offlineRender (*audiumEngine.getPlayListScheduler());

        audiumEngine.getLinkAudioDevice()->setBypass(true);
        audiumEngine.getPlayListScheduler()->prepareToPlay(config->blockSize, config->sampleRate);

        auto written = writeOutput(updateProgress);

        // change back to device settings
        if (auto device = audiumEngine.getAudioDeviceManager()->getCurrentAudioDevice()) {
            auto numSamples = device->getCurrentBufferSizeSamples();
            auto sampleRate = device->getCurrentSampleRate();
            audiumEngine.getPlayListScheduler()->prepareToPlay(numSamples, sampleRate);
        }
        audiumEngine.getLinkAudioDevice()->setBypass(false);

        return written;
    }

    /** The file a multi-mono export writes for one channel (1-based):
        "mix.flac", channel 3 -> "mix-03.flac". */
    static juce::File monoFileFor(const juce::File &target, int channel, ExportFormat format)
    {
        return target.getSiblingFile(target.getFileNameWithoutExtension() + "-"
                                     + juce::String(formatInteger(channel)) + fileExtension(format));
    }

private:
    /** Renders into a temporary file beside the target and only then swaps
        it in, so a failed or cancelled bounce leaves no partial file. A
        multi-mono export renders a WAV intermediate instead and encodes the
        mono files from it, so the channel limit of the target format (FLAC:
        eight) does not apply to the intermediate. */
    bool writeOutput(const std::function<void()> &updateProgress)
    {
        using Failure = ExportAudioConfig::Failure;

        if (isLossy(config->format)) {
            const auto steps = qualityOptions(config->format);
            if (config->quality < -1 || config->quality >= steps.size())
                return fail(Failure::unsupportedFormat,
                            "unsupported quality " + juce::String(config->quality)
                                + " (" + aFileOf(config->format) + " offers "
                                + steps.joinIntoString(", ") + ")");
        }

        const auto depths = supportedBitDepths(config->format);
        if (! isLossy(config->format) && ! depths.contains(config->bitDepth)) {
            juce::String depthList;
            for (auto i = 0; i < depths.size(); i++)
                depthList << (i == 0 ? "" : i == depths.size() - 1 ? " or " : ", ") << depths[i];
            return fail(Failure::unsupportedFormat,
                        "unsupported bit depth " + juce::String(config->bitDepth)
                            + " (" + aFileOf(config->format) + " takes " + depthList + " bits)");
        }

        if (! config->multiMono && config->numChannels > maxChannels(config->format))
            return fail(Failure::unsupportedFormat,
                        aFileOf(config->format) + " cannot hold " + juce::String(config->numChannels)
                            + " channels (at most " + juce::String(maxChannels(config->format))
                            + "); export multi-mono or as WAV instead");

        const auto renderAs = config->multiMono ? ExportFormat::wav : config->format;
        auto renderFormat = createAudioFormat(renderAs);

        juce::TemporaryFile tempFile (config->fileName);
        std::unique_ptr<juce::OutputStream> outStream (tempFile.getFile().createOutputStream());
        if (outStream == nullptr)
            return fail(Failure::cannotOpenOutput,
                        "could not open " + config->fileName.getFullPathName() + " for writing");

        auto writer = renderFormat->createWriterFor (outStream, writerOptions(renderAs, config->numChannels));
        if (writer == nullptr)
            return fail(Failure::unsupportedFormat,
                        "could not create " + aFileOf(renderAs) + " with "
                            + juce::String(config->numChannels) + " channels at "
                            + juce::String(config->sampleRate) + " Hz");

        auto written = config->playListItem != nullptr
                     ? audiumEngine.getPlayListScheduler()->bouncePlayListItem(writer.get(), config, updateProgress)
                     : audiumEngine.getPlayListScheduler()->bounceProject(writer.get(), config, updateProgress);

        writer.reset(); // finalises the header

        if (config->userCanceled)
            return false;

        if (! written)
            return fail(Failure::writeFailed,
                        "could not write " + config->fileName.getFullPathName() + " (is the disk full?)");

        // the temporary file is only the intermediate here and goes with it
        if (config->multiMono)
            return splitIntoMonoFiles(tempFile.getFile(), updateProgress);

        if (! tempFile.overwriteTargetFileWithTemporary())
            return fail(Failure::writeFailed,
                        "could not replace " + config->fileName.getFullPathName());

        return true;
    }

    /** Splits the multichannel WAV intermediate into -01, -02, ... files of
        the target format beside the target file. All or nothing: a failure
        or a cancel also removes the mono files written so far. */
    bool splitIntoMonoFiles(const juce::File &intermediate, const std::function<void()> &updateProgress)
    {
        using Failure = ExportAudioConfig::Failure;

        // read back as WAV whatever its extension says: the temporary file
        // carries the target's extension
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatReader> reader;
        if (auto in = intermediate.createInputStream())
            reader.reset(wav.createReaderFor(in.release(), true));
        if (reader == nullptr)
            return fail(Failure::writeFailed,
                        "could not read back the bounce of " + config->fileName.getFullPathName()
                            + " to split it into mono files");

        const auto numChannels = (int)reader->numChannels;
        juce::AudioBuffer<float> buf(numChannels, (int)reader->lengthInSamples);
        reader->read(&buf, 0, (int)reader->lengthInSamples, 0, true, true);
        reader.reset(); // release the file before it is deleted

        auto monoFormat = createAudioFormat(config->format);
        juce::Array<juce::File> monoFiles;
        auto ok = true;

        for (auto c = 0; c < numChannels; c++) {

            updateProgress();
            if (config->userCanceled)
                break;

            auto monoFile = monoFileFor(config->fileName, c + 1, config->format);

            juce::TemporaryFile temp (monoFile);
            std::unique_ptr<juce::OutputStream> out (temp.getFile().createOutputStream());
            if (out == nullptr) {
                ok = fail(Failure::cannotOpenOutput, "could not open " + monoFile.getFullPathName() + " for writing");
                break;
            }

            auto monoWriter = monoFormat->createWriterFor (out, writerOptions(config->format, 1));
            if (monoWriter == nullptr) {
                ok = fail(Failure::unsupportedFormat, "could not create a mono " + formatName(config->format)
                                                          + " writer for " + monoFile.getFullPathName());
                break;
            }

            juce::AudioBuffer<float> monoBuf(1, buf.getNumSamples());
            monoBuf.copyFrom(0, 0, buf, c, 0, buf.getNumSamples());
            auto written = monoWriter->writeFromAudioSampleBuffer(monoBuf, 0, monoBuf.getNumSamples());
            monoWriter.reset();

            if (! written || ! temp.overwriteTargetFileWithTemporary()) {
                ok = fail(Failure::writeFailed, "could not write " + monoFile.getFullPathName());
                break;
            }

            monoFiles.add(monoFile);
        }

        if (! ok || config->userCanceled) {
            for (auto& monoFile : monoFiles)
                monoFile.deleteFile();
            return false;
        }

        return true;
    }

    /** How to write a file of the given format: the export's bit depth for
        a lossless format, its quality for a lossy one. The WAV intermediate
        of a lossy multi-mono export is written as 32-bit float, so the
        encoder gets the full resolution of the render. */
    juce::AudioFormatWriter::Options writerOptions(ExportFormat format, int numChannels) const
    {
        auto options = juce::AudioFormatWriter::Options{}.withSampleRate (config->sampleRate)
                                                         .withNumChannels (numChannels);
        if (isLossy(format))
            return options.withBitsPerSample (32)
                          .withQualityOptionIndex (config->quality >= 0 ? config->quality : defaultQuality(format));

        if (isLossy(config->format))
            return options.withBitsPerSample (32)
                          .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);

        return options.withBitsPerSample (config->bitDepth);
    }

    bool fail(ExportAudioConfig::Failure failure, const juce::String &error)
    {
        config->failure = failure;
        config->error = error;
        return false;
    }

    static std::string formatInteger(long num) {
        std::ostringstream oss;
        oss << std::setfill('0') << std::setw(2) << num;
        return oss.str();
    }

    AudiumEngine &audiumEngine;                 ///< Reference to the AudiumEngine instance.

    std::shared_ptr<ExportAudioConfig> config;  ///< The export configuration.

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioExporter)
};

} // namespace audium
