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

void AudioExportThread::bounceToFile(audium::ExportAudioConfig &config)
{
    audiumEngine.setBypass(true);
    audiumEngine.getPlayListScheduler()->prepareToPlay(config.sampleRate, config.blockSize);
        
    juce::TemporaryFile tempFile (config.fileName);
    std::unique_ptr<OutputStream> outStream (tempFile.getFile().createOutputStream());

    if (outStream != nullptr)
    {
        const StringPairArray metadata;
        WavAudioFormat wav;
        std::unique_ptr<AudioFormatWriter> writer (wav.createWriterFor (outStream.get(), config.sampleRate,
                                                                        config.numChannels, config.bitDepth,
                                                                        metadata, 0));
        if (writer != nullptr)
        {
            outStream.release();
            

            
            audiumEngine.getPlayListScheduler()->bounceToFile(writer.get(), config, [this](void) {
                setProgress(this->config.progress);
            });

            writer.reset();
            tempFile.overwriteTargetFileWithTemporary();
        }
    }
    
    // change back to device settings
    auto device = audiumEngine.getAudioDeviceManager()->getCurrentAudioDevice();
    auto numSamples = device->getCurrentBufferSizeSamples();
    auto sampleRate = device->getCurrentSampleRate();
    audiumEngine.getPlayListScheduler()->prepareToPlay(sampleRate, numSamples);
    audiumEngine.setBypass(false);
}
