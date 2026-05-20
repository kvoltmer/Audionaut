#pragma once

#include <JuceHeader.h>

static float genSaw(int s, int w)
{
    return (static_cast<float>(s) / static_cast<float>(w - 1) * 2.f) - 1.f;
}

static const File createSlowSawAudioFile(int numSaws = 1,
                                         bool addSilenceAtStart = true,
                                         bool addSilenceAtEmd = true)
{
    auto targetFile = File(String(CURRENT_SOURCE_DIR) + String("/TestFiles/slow-saw.wav"));
    TemporaryFile tempFile (targetFile);

    std::unique_ptr<OutputStream> stream (tempFile.getFile().createOutputStream());
    jassert(stream);
    if (stream != nullptr) {
        WavAudioFormat wav;
        auto opt = AudioFormatWriter::Options{}.withSampleRate (44100.0)
            .withNumChannels (1)
            .withBitsPerSample (32)
            .withSampleFormat(AudioFormatWriterOptions::SampleFormat::floatingPoint);
        auto writer = wav.createWriterFor (stream, opt);
        jassert(writer);
        if (writer != nullptr) {

            auto blockSize = 44100;

            AudioBuffer<float> buffer(1, blockSize);
            AudioSourceChannelInfo info (&buffer, 0, blockSize);

            // start with 1 second silence
            if (addSilenceAtStart) {
                info.clearActiveBufferRegion();
                writer->writeFromAudioSampleBuffer(*info.buffer, info.startSample, info.numSamples);
            }
            
            // generate number of saw(s) [-1, +1]
            for (auto i = 0; i < numSaws; i++) {
                
                for (auto s = 0; s < blockSize; s++) {
                    *info.buffer->getWritePointer(0, s) = genSaw(s, blockSize);
                }
                writer->writeFromAudioSampleBuffer(*info.buffer, info.startSample, info.numSamples);
            }

            // add 1 second silence at the end
            if (addSilenceAtEmd) {
                info.clearActiveBufferRegion();
                writer->writeFromAudioSampleBuffer(*info.buffer, info.startSample, info.numSamples);
            }
            writer.reset();
            tempFile.overwriteTargetFileWithTemporary();
            return tempFile.getTargetFile();
        }
    }
    return File();
}

static const File createSlowSawTwoSecondsAudioFile()
{
    auto targetFile = File(String(CURRENT_SOURCE_DIR) + String("/TestFiles/slow-saw-2-seconds.wav"));
    TemporaryFile tempFile (targetFile);
    
    std::unique_ptr<OutputStream> stream (tempFile.getFile().createOutputStream());
    jassert(stream);
    if (stream != nullptr) {
        WavAudioFormat wav;
        auto opt = AudioFormatWriter::Options{}.withSampleRate (44100.0)
            .withNumChannels (1)
            .withBitsPerSample (32)
            .withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);
        auto writer = wav.createWriterFor (stream, opt);
        jassert(writer);
        if (writer != nullptr) {
            
            auto blockSize = 44100;
            
            AudioBuffer<float> buffer(1, blockSize);
            AudioSourceChannelInfo info (&buffer, 0, blockSize);
            
            // 1st second generate saw [-1, +1]
            for (auto s = 0; s < blockSize; s++) {
                *info.buffer->getWritePointer(0, s) = genSaw(s, blockSize);
            }
            writer->writeFromAudioSampleBuffer(*info.buffer, info.startSample, info.numSamples);
            
            // 2nd second silence
            info.clearActiveBufferRegion();
            writer->writeFromAudioSampleBuffer(*info.buffer, info.startSample, info.numSamples);
            
            writer.reset();
            tempFile.overwriteTargetFileWithTemporary();
            return tempFile.getTargetFile();
        }
    }
    return File();
}

static const File generateDcOffsetAudioFile(double lengthInSeconds)
{
    auto targetFile = File(String(CURRENT_SOURCE_DIR) + String("/TestFiles/dc-offset.wav"));
    TemporaryFile tempFile (targetFile);
    
    std::unique_ptr<OutputStream> stream (tempFile.getFile().createOutputStream());
    jassert(stream);
    if (stream != nullptr) {
        WavAudioFormat wav;
        auto opt = AudioFormatWriter::Options{}.withSampleRate (44100.0)
            .withNumChannels (1)
            .withBitsPerSample (32)
            .withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);
        auto writer = wav.createWriterFor (stream, opt);
        jassert(writer);
        if (writer != nullptr) {
            
            auto blockSize = static_cast<int>(44100.0 * lengthInSeconds);
            
            AudioBuffer<float> buffer(1, blockSize);
            AudioSourceChannelInfo info (&buffer, 0, blockSize);
            
            // generate dc offset [+1]
            for (auto s = 0; s < blockSize; s++) {
                *info.buffer->getWritePointer(0, s) = 1.f;
            }
            writer->writeFromAudioSampleBuffer(*info.buffer, info.startSample, info.numSamples);
            
            writer.reset();
            tempFile.overwriteTargetFileWithTemporary();
            return tempFile.getTargetFile();
        }
    }
    return File();
}

static AudioBuffer<float> audioFileToAudioBuffer(const juce::File fileName)
{
    AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    
    std::unique_ptr<AudioFormatReader> reader;
    reader.reset(formatManager.createReaderFor(fileName));
    
    jassert(reader);
    
    AudioBuffer<float> buffer((int)reader->numChannels, (int)reader->lengthInSamples);
    
    auto success = reader->read(&buffer,
                                0,
                                (int)reader->lengthInSamples,
                                0,
                                true,
                                true);
    jassert(success);
    return buffer;
}
