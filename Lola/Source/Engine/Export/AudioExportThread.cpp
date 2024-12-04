/*
  ==============================================================================

    AudioExportThread.cpp
    Created: 31 Oct 2024 9:40:47am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudioExportThread.h"
#include "Engine/PlayList/PlayListScheduler.h"

using namespace juce;

std::string formatInteger(long num) {
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << num;
    return oss.str();
};

void AudioExportThread::bounceToFile(audium::ExportAudioConfig &theConfiguration)
{
    config = theConfiguration;
    
    audiumEngine.setBypass(true);
    audiumEngine.getPlayListScheduler()->prepareToPlay(config.blockSize, config.sampleRate);
        
    TemporaryFile tempFile (config.fileName);
    std::unique_ptr<OutputStream> outStream (tempFile.getFile().createOutputStream());

    if (outStream != nullptr) {
        const StringPairArray metadata;
        WavAudioFormat wav;
        std::unique_ptr<AudioFormatWriter> writer (wav.createWriterFor (outStream.release(), config.sampleRate,
                                                                        (unsigned int)config.numChannels,
                                                                        config.bitDepth,
                                                                        metadata, 0));
        if (writer != nullptr) {
            audiumEngine.getPlayListScheduler()->bounceToFile(writer.get(), config, [this](void) {
                setProgress(this->config.progress);
            });

            writer.reset();
            tempFile.overwriteTargetFileWithTemporary();
        }
    }
    
    if (config.multiMono) {
        // split multi-channel created above into individual mono files
        
        AudioFormatManager formatManager;
        formatManager.registerBasicFormats();
        std::unique_ptr<AudioFormatReader> reader;
        reader.reset(formatManager.createReaderFor(config.fileName));
        if (reader != nullptr) {
            AudioBuffer<float> buf((int)reader->numChannels, (int)reader->lengthInSamples);
            reader->read(&buf, 0, (int)reader->lengthInSamples, 0, true, true);
            
            for (auto c = 0; c < reader->numChannels; c++) {
                auto trackNumber = formatInteger(c + 1);
                auto siblingName = config.fileName.getFileNameWithoutExtension() + "-" + String(trackNumber) + ".wav";
                
                TemporaryFile temp (config.fileName.getSiblingFile(siblingName));
                std::unique_ptr<OutputStream> outStream (temp.getFile().createOutputStream());
                if (outStream != nullptr) {
                    const StringPairArray metadata;
                    WavAudioFormat wav;
                    std::unique_ptr<AudioFormatWriter> writer (wav.createWriterFor (outStream.release(), config.sampleRate,
                                                                                    1,
                                                                                    config.bitDepth,
                                                                                    metadata, 0));
                    if (writer != nullptr) {
                        AudioBuffer<float> monoBuf(1, buf.getNumSamples());
                        monoBuf.copyFrom(0, 0, buf, c, 0, buf.getNumSamples());
                        writer->writeFromAudioSampleBuffer(monoBuf, 0, monoBuf.getNumSamples());
                        writer.reset();
                        temp.overwriteTargetFileWithTemporary();
                    }
                }
            }
        }
        
        File sourceFile(config.fileName);
        sourceFile.deleteFile();
        
    }
    
    
    // change back to device settings
    if (auto device = audiumEngine.getAudioDeviceManager()->getCurrentAudioDevice())
    {
        auto numSamples = device->getCurrentBufferSizeSamples();
        auto sampleRate = device->getCurrentSampleRate();
        audiumEngine.getPlayListScheduler()->prepareToPlay(numSamples, sampleRate);
    }
    audiumEngine.setBypass(false);
}
