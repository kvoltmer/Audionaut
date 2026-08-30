//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Engine/AudiumEngine.h"
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
     */
    void bounce(const std::function<bool(double)> &shouldContinue = {})
    {
        auto updateProgress = [this, &shouldContinue]() {
            if (shouldContinue != nullptr && !shouldContinue(config->progress))
                config->userCanceled = true;
        };

        audiumEngine.getLinkAudioDevice()->setBypass(true);
        audiumEngine.getPlayListScheduler()->prepareToPlay(config->blockSize, config->sampleRate);

        juce::TemporaryFile tempFile (config->fileName);
        std::unique_ptr<juce::OutputStream> outStream (tempFile.getFile().createOutputStream());

        if (outStream != nullptr) {
            juce::WavAudioFormat wav;
            auto opt = juce::AudioFormatWriter::Options{}.withSampleRate (config->sampleRate)
                                                          .withNumChannels (config->numChannels)
                                                          .withBitsPerSample (config->bitDepth);
            auto writer = wav.createWriterFor (outStream, opt);
            if (writer != nullptr) {

                if (config->playListItem != nullptr) {
                    audiumEngine.getPlayListScheduler()->bouncePlayListItem(writer.get(), config, updateProgress);
                }
                else {
                    audiumEngine.getPlayListScheduler()->bounceProject(writer.get(), config, updateProgress);
                }

                writer.reset();

                if (!config->userCanceled)
                    tempFile.overwriteTargetFileWithTemporary();
            }
        }

        if (config->multiMono && !config->userCanceled) {
            // split multi-channel created above into individual mono files

            juce::AudioFormatManager formatManager;
            formatManager.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> reader;
            reader.reset(formatManager.createReaderFor(config->fileName));
            if (reader != nullptr) {
                juce::AudioBuffer<float> buf((int)reader->numChannels, (int)reader->lengthInSamples);
                reader->read(&buf, 0, (int)reader->lengthInSamples, 0, true, true);

                for (auto c = 0; c < (int)reader->numChannels; c++) {

                    updateProgress();
                    if (config->userCanceled)
                        break;

                    auto trackNumber = formatInteger(c + 1);
                    auto siblingName = config->fileName.getFileNameWithoutExtension() + "-" + juce::String(trackNumber) + ".wav";

                    juce::TemporaryFile temp (config->fileName.getSiblingFile(siblingName));
                    std::unique_ptr<juce::OutputStream> out (temp.getFile().createOutputStream());
                    if (out != nullptr) {
                        juce::WavAudioFormat monoWav;
                        auto monoOpt = juce::AudioFormatWriter::Options{}.withSampleRate (config->sampleRate)
                                                                          .withNumChannels (1)
                                                                          .withBitsPerSample (config->bitDepth);
                        auto monoWriter = monoWav.createWriterFor (out, monoOpt);

                        if (monoWriter != nullptr) {
                            juce::AudioBuffer<float> monoBuf(1, buf.getNumSamples());
                            monoBuf.copyFrom(0, 0, buf, c, 0, buf.getNumSamples());
                            monoWriter->writeFromAudioSampleBuffer(monoBuf, 0, monoBuf.getNumSamples());
                            monoWriter.reset();
                            temp.overwriteTargetFileWithTemporary();
                        }
                    }
                }
            }

            juce::File sourceFile(config->fileName);
            sourceFile.deleteFile();
        }


        // change back to device settings
        if (auto device = audiumEngine.getAudioDeviceManager()->getCurrentAudioDevice()) {
            auto numSamples = device->getCurrentBufferSizeSamples();
            auto sampleRate = device->getCurrentSampleRate();
            audiumEngine.getPlayListScheduler()->prepareToPlay(numSamples, sampleRate);
        }
        audiumEngine.getLinkAudioDevice()->setBypass(false);
    }

private:
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
