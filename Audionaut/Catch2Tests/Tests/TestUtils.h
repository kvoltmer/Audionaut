#pragma once

#include <JuceHeader.h>

#include "Engine/Playback/RenderThreadPool.h"

/**
 * Whether a render pool's timing can be held to tight bounds here. The
 * pool's jobs and its wait burn wall time, so a thread pre-empted inside
 * a job reads a longer busy time than the job's own, and a wait longer
 * than two block periods counts a stall. Bounds that say the pool adds
 * little on top of the jobs hold only where every thread slot has a
 * physical core to itself; CI runners have two to four, and the logical
 * count overstates them. Lower bounds hold on any machine.
 */
inline bool everyThreadSlotHasACore (const audium::RenderThreadPool& pool)
{
    return juce::SystemStats::getNumPhysicalCpus() >= pool.getNumThreadSlots();
}

/// Where the tests write what nobody reads back from the repository:
/// generated input audio, bounces, scratch projects. One directory per run
/// of the test binary under the temp location, removed when the run exits
/// (a crashed run leaves it behind in temp). CI fails a run that leaves
/// files under TestFiles, so outputs never go there.
inline const File& testOutputDirectory()
{
    struct RunDirectory
    {
        File directory = File::getSpecialLocation(File::tempDirectory)
                             .getChildFile("AudionautTests-" + Uuid().toDashedString());

        RunDirectory()  { directory.createDirectory(); }
        ~RunDirectory() { directory.deleteRecursively(); }
    };

    static RunDirectory run;
    return run.directory;
}

static float genSaw(int s, int w)
{
    return (static_cast<float>(s) / static_cast<float>(w - 1) * 2.f) - 1.f;
}

static const File createSlowSawAudioFile(int numSaws = 1,
                                         bool addSilenceAtStart = true,
                                         bool addSilenceAtEmd = true)
{
    auto targetFile = testOutputDirectory().getChildFile("slow-saw.wav");
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
    auto targetFile = testOutputDirectory().getChildFile("slow-saw-2-seconds.wav");
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

/// A pure sine, optionally preceded by silence (for onset-alignment
/// probes). Written fresh on every call, like the saw generators. The
/// file's sample rate defaults to the 44.1 kHz the tests bounce at;
/// pass another one to exercise the clip chain's rate correction.
static const File createSineAudioFile(double frequencyHz, double lengthInSeconds,
                                      double leadingSilenceSeconds = 0.0,
                                      double sampleRate = 44100.0)
{
    auto targetFile = testOutputDirectory().getChildFile("sine-gen.wav");
    TemporaryFile tempFile (targetFile);

    std::unique_ptr<OutputStream> stream (tempFile.getFile().createOutputStream());
    jassert(stream);
    if (stream != nullptr) {
        WavAudioFormat wav;
        auto opt = AudioFormatWriter::Options{}.withSampleRate (sampleRate)
            .withNumChannels (1)
            .withBitsPerSample (32)
            .withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);
        auto writer = wav.createWriterFor (stream, opt);
        jassert(writer);
        if (writer != nullptr) {

            const auto silenceSamples = static_cast<int>(sampleRate * leadingSilenceSeconds);
            const auto sineSamples = static_cast<int>(sampleRate * lengthInSeconds);

            AudioBuffer<float> buffer(1, silenceSamples + sineSamples);
            buffer.clear();
            for (auto s = 0; s < sineSamples; s++)
                *buffer.getWritePointer(0, silenceSamples + s)
                    = std::sin(MathConstants<double>::twoPi * frequencyHz * s / sampleRate);

            writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());

            writer.reset();
            tempFile.overwriteTargetFileWithTemporary();
            return tempFile.getTargetFile();
        }
    }
    return File();
}

/// A constant +1.0, at the given file sample rate (defaults to the
/// 44.1 kHz the tests usually bounce at).
static const File generateDcOffsetAudioFile(double lengthInSeconds, double sampleRate = 44100.0)
{
    auto targetFile = testOutputDirectory().getChildFile("dc-offset.wav");
    TemporaryFile tempFile (targetFile);
    
    std::unique_ptr<OutputStream> stream (tempFile.getFile().createOutputStream());
    jassert(stream);
    if (stream != nullptr) {
        WavAudioFormat wav;
        auto opt = AudioFormatWriter::Options{}.withSampleRate (sampleRate)
            .withNumChannels (1)
            .withBitsPerSample (32)
            .withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);
        auto writer = wav.createWriterFor (stream, opt);
        jassert(writer);
        if (writer != nullptr) {
            
            auto blockSize = static_cast<int>(sampleRate * lengthInSeconds);
            
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

/// One linear ramp from -1 to +1 over the whole file (genSaw over its
/// length), at the given sample rate: every sample carries its own
/// position, so a bounce can be checked for which source sample landed
/// where.
static const File createRampAudioFile(double lengthInSeconds, double sampleRate = 44100.0)
{
    auto targetFile = testOutputDirectory().getChildFile("ramp-gen.wav");
    TemporaryFile tempFile (targetFile);

    std::unique_ptr<OutputStream> stream (tempFile.getFile().createOutputStream());
    jassert(stream);
    if (stream != nullptr) {
        WavAudioFormat wav;
        auto opt = AudioFormatWriter::Options{}.withSampleRate (sampleRate)
            .withNumChannels (1)
            .withBitsPerSample (32)
            .withSampleFormat(juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);
        auto writer = wav.createWriterFor (stream, opt);
        jassert(writer);
        if (writer != nullptr) {

            const auto numSamples = static_cast<int>(sampleRate * lengthInSeconds);

            AudioBuffer<float> buffer(1, numSamples);
            for (auto s = 0; s < numSamples; s++)
                *buffer.getWritePointer(0, s) = genSaw(s, numSamples);

            writer->writeFromAudioSampleBuffer(buffer, 0, numSamples);

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
