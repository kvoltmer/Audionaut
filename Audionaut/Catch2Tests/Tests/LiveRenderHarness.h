//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <catch2/catch_test_macros.hpp>

#include <JuceHeader.h>

#include <chrono>
#include <cmath>
#include <memory>

#include "Engine/AudiumEngine.h"
#include "Engine/Export/AudioExporter.h"
#include "Engine/Export/ExportAudioConfig.h"
#include "Engine/Link/LinkAudioDevice.h"
#include "Engine/Link/LinkEngine.hpp"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/Provider/TempoProvider.h"

// The live audio path, driven by hand.
//
// The app renders through LinkAudioDevice::audioDeviceIOCallbackWithContext:
// Link maps the callback's host time to a beat, the scheduler positions its
// voices from that beat, and the bus renderer mixes them into the device's
// output arrays. The bounce (PlayListScheduler::bounceProject) drives the
// same scheduler with a beat count it advances itself, offline. Everything
// here exists so a test can run the *live* half of that without a device:
//
//  - FakeAudioIODevice: what audioDeviceAboutToStart reads off a device.
//  - LiveBlockDriver:   the device callback block by block, with the beat
//                       coming out of the real LinkEngine.
//  - bounceProject():   the offline reference for the same span.
//  - compareBuffers():  the sample-level comparison with diagnostics.

namespace audium::test {

/// Just enough of an AudioIODevice for audioDeviceAboutToStart: the device
/// never runs - the callback is driven by hand (see LiveBlockDriver).
class FakeAudioIODevice : public juce::AudioIODevice
{
public:
    FakeAudioIODevice (double sampleRate_, int blockSize_, int numInputs_ = 2, int numOutputs_ = 2) :
        juce::AudioIODevice ("fake", "fake"),
        sampleRate (sampleRate_), blockSize (blockSize_),
        numInputs (numInputs_), numOutputs (numOutputs_) {}

    juce::StringArray getOutputChannelNames() override { return channelNames ("out ", numOutputs); }
    juce::StringArray getInputChannelNames() override  { return channelNames ("in ", numInputs); }
    juce::Array<double> getAvailableSampleRates() override { return { sampleRate }; }
    juce::Array<int> getAvailableBufferSizes() override    { return { blockSize }; }
    int getDefaultBufferSize() override                    { return blockSize; }
    juce::String open (const juce::BigInteger&, const juce::BigInteger&, double, int) override { return {}; }
    void close() override {}
    bool isOpen() override { return true; }
    void start (juce::AudioIODeviceCallback*) override {}
    void stop() override {}
    bool isPlaying() override { return false; }
    juce::String getLastError() override { return {}; }
    int getCurrentBufferSizeSamples() override { return blockSize; }
    double getCurrentSampleRate() override     { return sampleRate; }
    int getCurrentBitDepth() override          { return 32; }
    juce::BigInteger getActiveOutputChannels() const override { return range (numOutputs); }
    juce::BigInteger getActiveInputChannels() const override  { return range (numInputs); }
    int getOutputLatencyInSamples() override { return 0; }
    int getInputLatencyInSamples() override  { return 0; }

    const double sampleRate;
    const int blockSize;
    const int numInputs;
    const int numOutputs;

private:
    static juce::StringArray channelNames (const juce::String& prefix, int count)
    {
        juce::StringArray names;
        for (auto i = 0; i < count; ++i)
            names.add (prefix + juce::String (i + 1));
        return names;
    }

    static juce::BigInteger range (int count)
    {
        juce::BigInteger bits;
        bits.setRange (0, count, true);
        return bits;
    }
};

/**
 * Drives an engine's live render path block by block, as the device would.
 *
 * Construction runs the device's audioDeviceAboutToStart (Link sample rate,
 * prepareToPlay of the whole scheduler graph, output latency). play() is the
 * transport's Play button. render() then runs one device callback per block:
 * the LinkEngine's audio callback (which is where Play and Stop requests
 * take effect and the start beat gets pinned to the first block), the beat
 * for the block, a cleared output, and PlayListScheduler::process with a
 * ProcessContextNonReplacing over the device's input and output arrays -
 * the body of LinkAudioDevice::audioDeviceIOCallbackWithContext.
 *
 * What the driver cannot borrow from the real callback is its clock: the
 * device callback maps its sample time to a host time through Link's
 * HostTimeFilter, a regression against the wall clock. Driven in a loop
 * that timeline crawls (or, paced in real time, jitters by samples), so
 * the driver feeds the LinkEngine the host times a jitter-free device
 * would report: one block's duration apart, rounded to the microsecond
 * Link works in. The one block that is clock-independent - the first after
 * Play, whose host time Link pins to the start beat - can be rendered
 * through the real callback with deviceCallbackBlock().
 *
 * Between blocks the message thread gets a turn every pumpEveryBlocks
 * blocks (the loop wrap notifications and other async updates the audio
 * thread posts), the way it would while the device runs.
 */
class LiveBlockDriver
{
public:
    LiveBlockDriver (AudiumEngine& engine, FakeAudioIODevice& device_) :
        device (device_),
        linkDevice (engine.getLinkAudioDevice()),
        scheduler (engine.getPlayListScheduler()),
        linkEngine (scheduler->getLinkEngine()),
        input (device_.numInputs, device_.blockSize),
        output (device_.numOutputs, device_.blockSize)
    {
        input.clear();
        // near "now", like the real callback's host times; Link's timeline
        // arithmetic works on absolute microseconds
        hostTimeOrigin = ableton::link::platform::Clock{}.micros();
        linkDevice->audioDeviceAboutToStart (&device);

        // the device runs from here on, transport stopped: what a bounce or
        // an earlier Stop left requested in the LinkEngine is taken by these
        // callbacks, as in the app, not by the first block after Play
        idle (2);
    }

    ~LiveBlockDriver()
    {
        linkDevice->audioDeviceStopped();
    }

    /// Device callbacks with nothing to play (between Stop and Play).
    void idle (int numBlocks)
    {
        for (auto i = 0; i < numBlocks; ++i)
            renderBlock (device.blockSize);
    }

    /// The transport's Play button: start position, then startPlaying
    /// (commit, standby primes, Link start request, loop reset).
    void play (double startSeconds)
    {
        scheduler->setAbsoluteStartPosition (startSeconds, audium::seconds);
        scheduler->startPlaying();
    }

    /// The transport's Stop button, followed by the one callback the device
    /// still runs afterwards: Link takes the stop request there and the
    /// stop-all-voices command drains.
    void stop()
    {
        scheduler->stopPlaying();
        renderBlock (device.blockSize);
    }

    /// Runs whole device blocks until `seconds` of output are captured and
    /// returns them (trimmed to the exact sample count).
    juce::AudioBuffer<float> render (double seconds)
    {
        const auto totalSamples = static_cast<int> (std::llround (seconds * device.sampleRate));
        const auto numBlocks = (totalSamples + device.blockSize - 1) / device.blockSize;

        juce::AudioBuffer<float> captured (device.numOutputs, numBlocks * device.blockSize);
        captured.clear();

        for (auto block = 0; block < numBlocks; ++block)
        {
            renderBlock (device.blockSize);
            for (auto c = 0; c < device.numOutputs; ++c)
                captured.copyFrom (c, block * device.blockSize, output, c, 0, device.blockSize);

            if (pumpEveryBlocks > 0 && (block + 1) % pumpEveryBlocks == 0)
                pumpMessageThread();
        }
        pumpMessageThread();

        captured.setSize (device.numOutputs, totalSamples, true);
        return captured;
    }

    /// One block through the real LinkAudioDevice::audioDeviceIOCallbackWithContext.
    /// Only the first block after play() renders a defined position (see the
    /// class comment); the driver's own clock is not advanced.
    juce::AudioBuffer<float> deviceCallbackBlock()
    {
        output.clear();
        linkDevice->audioDeviceIOCallbackWithContext (input.getArrayOfReadPointers(),
                                                      device.numInputs,
                                                      output.getArrayOfWritePointers(),
                                                      device.numOutputs,
                                                      device.blockSize,
                                                      {});
        return output;
    }

    /// The beat the driver handed the scheduler for the last block.
    double lastBeats() const noexcept { return beats; }

    /// Whether Link reported the transport playing for the last block.
    bool lastIsPlaying() const noexcept { return isPlaying; }

    int blocksRendered() const noexcept { return blockCount; }

    /// How often the message thread runs between blocks; 0 = only after render().
    int pumpEveryBlocks = 8;

private:
    void renderBlock (int numSamples)
    {
        using namespace std::chrono;

        // the host time a jitter-free device reports for this block's first sample
        const auto hostTime = hostTimeOrigin
            + microseconds (std::llround (static_cast<double> (sampleTime) * 1.0e6 / device.sampleRate));
        const auto bufferBeginAtOutput = hostTime + linkEngine->mOutputLatency.load();

        isPlaying = linkEngine->audioCallback (bufferBeginAtOutput, static_cast<std::size_t> (numSamples));
        beats = linkEngine->beatAtTime (bufferBeginAtOutput, linkEngine->quantum());

        // the device callback clears its output before rendering into it
        output.clear();
        juce::dsp::AudioBlock<const float> in (input.getArrayOfReadPointers(),
                                               static_cast<size_t> (device.numInputs),
                                               static_cast<size_t> (numSamples));
        juce::dsp::AudioBlock<float> out (output.getArrayOfWritePointers(),
                                          static_cast<size_t> (device.numOutputs),
                                          static_cast<size_t> (numSamples));
        juce::dsp::ProcessContextNonReplacing<float> context (in, out);

        scheduler->process (context, isPlaying, beats, numSamples);

        sampleTime += static_cast<juce::uint64> (numSamples);
        ++blockCount;
    }

    static void pumpMessageThread()
    {
        if (auto* mm = juce::MessageManager::getInstanceWithoutCreating())
            if (mm->isThisTheMessageThread())
                mm->runDispatchLoopUntil (0);
    }

    FakeAudioIODevice& device;
    std::shared_ptr<LinkAudioDevice> linkDevice;
    std::shared_ptr<PlayListScheduler> scheduler;
    std::shared_ptr<LinkEngine> linkEngine;

    juce::AudioBuffer<float> input;
    juce::AudioBuffer<float> output;

    std::chrono::microseconds hostTimeOrigin { 0 };
    juce::uint64 sampleTime = 0;
    double beats = 0.0;
    bool isPlaying = false;
    int blockCount = 0;
};

/// The offline reference: bounceProject over the same span, through the
/// AudioExporter (bypass, offline render timing, prepareToPlay at the
/// bounce's block size), written as 32-bit float so the file round trip is
/// exact. Returns the rendered audio.
inline juce::AudioBuffer<float> bounceProject (AudiumEngine& engine,
                                               double startSeconds, double lengthSeconds,
                                               int blockSize, double sampleRate, int numChannels)
{
    auto config = std::make_shared<ExportAudioConfig>();
    config->fileName = juce::File::createTempFile ("-live-equivalence.wav");
    config->sampleRate = sampleRate;
    config->blockSize = blockSize;
    config->numChannels = numChannels;
    config->bitDepth = 32;          // JUCE writes 32-bit WAV as float
    config->positionSeconds = startSeconds;
    config->lengthSeconds = lengthSeconds;

    const auto written = AudioExporter (engine, config).bounce();
    CAPTURE (config->error);
    REQUIRE (written);

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (config->fileName));
    REQUIRE (reader != nullptr);
    juce::AudioBuffer<float> buffer (static_cast<int> (reader->numChannels), static_cast<int> (reader->lengthInSamples));
    REQUIRE (reader->read (&buffer, 0, buffer.getNumSamples(), 0, true, true));
    reader.reset();
    config->fileName.deleteFile();
    return buffer;
}

/// Where and by how much two renders differ.
struct BufferDifference
{
    float maxAbs = 0.0f;        ///< largest |a - b| over the compared range
    int atSample = -1;          ///< where it occurs
    int atChannel = -1;
    int firstOverTolerance = -1;///< first sample whose difference exceeds the tolerance, -1 if none
    float referenceMagnitude = 0.0f; ///< peak of the reference over the range - to make sure something was compared
};

/// Compares `live` against `reference` sample by sample over [from, to)
/// (to < 0: the whole buffer) on every channel.
inline BufferDifference compareBuffers (const juce::AudioBuffer<float>& live,
                                        const juce::AudioBuffer<float>& reference,
                                        float tolerance, int from = 0, int to = -1)
{
    REQUIRE (live.getNumChannels() == reference.getNumChannels());
    REQUIRE (live.getNumSamples() == reference.getNumSamples());
    if (to < 0)
        to = live.getNumSamples();

    BufferDifference result;
    for (auto c = 0; c < live.getNumChannels(); ++c)
    {
        result.referenceMagnitude = std::max (result.referenceMagnitude, reference.getMagnitude (c, from, to - from));
        for (auto i = from; i < to; ++i)
        {
            const auto diff = std::abs (live.getSample (c, i) - reference.getSample (c, i));
            if (diff > result.maxAbs)
            {
                result.maxAbs = diff;
                result.atSample = i;
                result.atChannel = c;
            }
            if (diff > tolerance && (result.firstOverTolerance < 0 || i < result.firstOverTolerance))
                result.firstOverTolerance = i;
        }
    }
    return result;
}

} // namespace audium::test
