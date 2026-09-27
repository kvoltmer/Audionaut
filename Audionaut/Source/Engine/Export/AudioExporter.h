//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Engine/AudiumEngine.h"
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

private:
    /** Renders into a temporary file beside the target and only then swaps
        it in, so a failed or cancelled bounce leaves no partial file. */
    bool writeOutput(const std::function<void()> &updateProgress)
    {
        using Failure = ExportAudioConfig::Failure;

        juce::WavAudioFormat wav;
        if (! wav.getPossibleBitDepths().contains(config->bitDepth))
            return fail(Failure::unsupportedFormat,
                        "unsupported bit depth " + juce::String(config->bitDepth)
                            + " (a WAV file takes 8, 16, 24 or 32 bits)");

        juce::TemporaryFile tempFile (config->fileName);
        std::unique_ptr<juce::OutputStream> outStream (tempFile.getFile().createOutputStream());
        if (outStream == nullptr)
            return fail(Failure::cannotOpenOutput,
                        "could not open " + config->fileName.getFullPathName() + " for writing");

        auto opt = juce::AudioFormatWriter::Options{}.withSampleRate (config->sampleRate)
                                                      .withNumChannels (config->numChannels)
                                                      .withBitsPerSample (config->bitDepth);
        auto writer = wav.createWriterFor (outStream, opt);
        if (writer == nullptr)
            return fail(Failure::unsupportedFormat,
                        "a WAV file cannot hold " + juce::String(config->numChannels) + " channels");

        auto written = config->playListItem != nullptr
                     ? audiumEngine.getPlayListScheduler()->bouncePlayListItem(writer.get(), config, updateProgress)
                     : audiumEngine.getPlayListScheduler()->bounceProject(writer.get(), config, updateProgress);

        writer.reset(); // finalises the header

        if (config->userCanceled)
            return false;

        if (! written)
            return fail(Failure::writeFailed,
                        "could not write " + config->fileName.getFullPathName() + " (is the disk full?)");

        if (! tempFile.overwriteTargetFileWithTemporary())
            return fail(Failure::writeFailed,
                        "could not replace " + config->fileName.getFullPathName());

        return config->multiMono ? splitIntoMonoFiles(updateProgress) : true;
    }

    /** Splits the multichannel file written above into -01.wav, -02.wav, ...
        beside it and removes it. All or nothing: a failure or a cancel also
        removes the mono files written so far. */
    bool splitIntoMonoFiles(const std::function<void()> &updateProgress)
    {
        using Failure = ExportAudioConfig::Failure;

        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor(config->fileName));
        if (reader == nullptr) {
            config->fileName.deleteFile();
            return fail(Failure::writeFailed,
                        "could not read back " + config->fileName.getFullPathName() + " to split it into mono files");
        }

        const auto numChannels = (int)reader->numChannels;
        juce::AudioBuffer<float> buf(numChannels, (int)reader->lengthInSamples);
        reader->read(&buf, 0, (int)reader->lengthInSamples, 0, true, true);
        reader.reset(); // release the file before it is deleted below

        juce::Array<juce::File> monoFiles;
        auto ok = true;

        for (auto c = 0; c < numChannels; c++) {

            updateProgress();
            if (config->userCanceled)
                break;

            auto trackNumber = formatInteger(c + 1);
            auto siblingName = config->fileName.getFileNameWithoutExtension() + "-" + juce::String(trackNumber) + ".wav";
            auto monoFile = config->fileName.getSiblingFile(siblingName);

            juce::TemporaryFile temp (monoFile);
            std::unique_ptr<juce::OutputStream> out (temp.getFile().createOutputStream());
            if (out == nullptr) {
                ok = fail(Failure::cannotOpenOutput, "could not open " + monoFile.getFullPathName() + " for writing");
                break;
            }

            juce::WavAudioFormat monoWav;
            auto monoOpt = juce::AudioFormatWriter::Options{}.withSampleRate (config->sampleRate)
                                                              .withNumChannels (1)
                                                              .withBitsPerSample (config->bitDepth);
            auto monoWriter = monoWav.createWriterFor (out, monoOpt);
            if (monoWriter == nullptr) {
                ok = fail(Failure::unsupportedFormat, "could not create a mono WAV writer for " + monoFile.getFullPathName());
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

        // the multichannel file was only the intermediate
        config->fileName.deleteFile();

        if (! ok || config->userCanceled) {
            for (auto& monoFile : monoFiles)
                monoFile.deleteFile();
            return false;
        }

        return true;
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
