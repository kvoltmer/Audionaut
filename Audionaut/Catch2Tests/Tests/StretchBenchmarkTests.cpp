//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <vector>

#include "Engine/AudioSources/StretchAudioSource.h"

#if JUCE_MAC
 #include <mach/mach.h>
#endif

// The stretch node's buffer sizing, and its cost on this machine.
//
// The first scenario pins the sizing contract: Rubber Band releases output
// one hop at a time and only once it holds a full window, so a prime or a
// per-block pull sized on the exact ratio comes up short - and a short FIFO
// pads with silence, which shifts the clip. Every (rate, block, ratio)
// combination must render without a single underrun or dropped sample.
//
// The benchmark scenario is hidden ([.]) - run on demand with
// `AudionautTests "[bench]"`; it prints how long a voice takes to prepare,
// what the prime after a position jump costs inside one audio callback,
// the steady-state CPU share per voice at each ratio, and the memory per
// voice. Printed, not asserted, apart from sanity bounds.

using namespace audium;

namespace {

// A 441 Hz tone from a precomputed table, so the source costs next to
// nothing and the numbers are the stretch node's.
class SineSource : public juce::AudioSource
{
public:
    void prepareToPlay (int, double) override
    {
        table.resize (100);
        for (size_t i = 0; i < table.size(); ++i)
            table[i] = static_cast<float> (std::sin (juce::MathConstants<double>::twoPi * static_cast<double> (i) / static_cast<double> (table.size())));
    }

    void releaseResources() override {}

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& info) override
    {
        for (int i = 0; i < info.numSamples; ++i)
        {
            const auto value = table[position];
            position = (position + 1) % table.size();
            for (int channel = 0; channel < info.buffer->getNumChannels(); ++channel)
                info.buffer->setSample (channel, info.startSample + i, value);
        }
    }

private:
    std::vector<float> table;
    size_t position = 0;
};

using Clock = std::chrono::steady_clock;

/// Resident memory of this process in bytes (0 where unsupported).
double residentBytes()
{
   #if JUCE_MAC
    mach_task_basic_info info {};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info (mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t> (&info), &count) == KERN_SUCCESS)
        return static_cast<double> (info.resident_size);
   #endif
    return 0.0;
}

double millisecondsSince (Clock::time_point start)
{
    return std::chrono::duration<double, std::milli> (Clock::now() - start).count();
}

} // namespace

SCENARIO ("the stretch node never underruns after a prime", "[engine][clipspeed][stretch]")
{
    for (const auto sampleRate : { 44100.0, 48000.0, 96000.0 })
        for (const auto blockSize : { 64, 128, 512, 2048 })
        {
            DYNAMIC_SECTION (static_cast<int> (sampleRate) << " Hz, block " << blockSize)
            {
                SineSource sine;
                StretchAudioSource node (&sine, 2);
                node.prepareToPlay (blockSize, sampleRate);
                node.setEnabled (true);

                juce::AudioBuffer<float> out (2, blockSize);
                const auto blocksPerPhase = static_cast<int> (sampleRate) / blockSize / 4;   // a quarter second

                for (const auto ratio : { 0.25, 0.4, 0.5, 0.75, 1.0, 1.5, 2.0, 3.0, 4.0 })
                {
                    node.setSpeedRatio (ratio);

                    // twice: the second prime restarts a stretcher that holds a stream
                    for (int repeat = 0; repeat < 2; ++repeat)
                    {
                        node.flushBuffers();
                        for (int block = 0; block < blocksPerPhase; ++block)
                            node.getNextAudioBlock (juce::AudioSourceChannelInfo (out));
                    }

                    // a ratio change mid-stream, without a re-prime
                    node.setSpeedRatio (ratio * 1.5 <= 4.0 ? ratio * 1.5 : ratio / 1.5);
                    for (int block = 0; block < blocksPerPhase; ++block)
                        node.getNextAudioBlock (juce::AudioSourceChannelInfo (out));

                    CAPTURE (ratio);
                    REQUIRE (node.getUnderruns() == 0);
                    REQUIRE (node.getOverflows() == 0);
                    REQUIRE (out.getMagnitude (0, blockSize) > 0.5f);
                }
            }
        }
}

SCENARIO ("stretch node cost per voice", "[.][stretch][bench]")
{
    constexpr int numChannels = 2;
    constexpr int blockSize = 512;
    constexpr double sampleRate = 44100.0;
    constexpr double renderSeconds = 20.0;

    std::ostringstream report;
    report << std::fixed << std::setprecision (2);

    SineSource sine;
    StretchAudioSource node (&sine, numChannels);

    auto started = Clock::now();
    node.prepareToPlay (blockSize, sampleRate);
    report << "prepareToPlay: " << millisecondsSince (started) << " ms\n";

    juce::AudioBuffer<float> out (numChannels, blockSize);
    node.setEnabled (true);

    for (const auto ratio : { 0.25, 0.5, 0.75, 1.0, 1.5, 2.0, 4.0 })
    {
        node.setSpeedRatio (ratio);

        // the first block after a position jump carries the prime: median
        // of several, the first touch of a buffer page is not the cost
        std::vector<double> primes;
        for (int repeat = 0; repeat < 9; ++repeat)
        {
            node.flushBuffers();
            started = Clock::now();
            node.getNextAudioBlock (juce::AudioSourceChannelInfo (out));
            primes.push_back (millisecondsSince (started));
        }
        std::sort (primes.begin(), primes.end());
        const auto primeMs = primes[primes.size() / 2];
        const auto underrunsAfterPrime = node.getUnderruns();

        const auto numBlocks = static_cast<int> (renderSeconds * sampleRate / blockSize);
        started = Clock::now();
        for (int block = 0; block < numBlocks; ++block)
            node.getNextAudioBlock (juce::AudioSourceChannelInfo (out));
        const auto renderMs = millisecondsSince (started);

        const auto outputMs = numBlocks * blockSize * 1000.0 / sampleRate;
        const auto blockMs = blockSize * 1000.0 / sampleRate;

        report << "ratio x" << ratio
               << ":  prime " << std::setw (6) << primeMs << " ms (" << std::setw (5) << primeMs / blockMs << " blocks)"
               << "   steady " << std::setw (6) << 100.0 * renderMs / outputMs << " % of one core"
               << "  (" << std::setw (6) << outputMs / renderMs << "x realtime)"
               << "  underruns " << underrunsAfterPrime << "/" << node.getUnderruns() << "\n";

        REQUIRE (out.getMagnitude (0, blockSize) > 0.5f);   // it did render audio
        REQUIRE (node.getOverflows() == 0);
    }

    // memory: what a project full of stretch voices costs once every voice
    // has been through a prime (pages touched)
    {
        constexpr int numVoices = 16;
        std::vector<std::unique_ptr<SineSource>> sources;
        std::vector<std::unique_ptr<StretchAudioSource>> nodes;
        const auto before = residentBytes();

        for (int i = 0; i < numVoices; ++i)
        {
            sources.push_back (std::make_unique<SineSource>());
            nodes.push_back (std::make_unique<StretchAudioSource> (sources.back().get(), numChannels));
            nodes.back()->prepareToPlay (blockSize, sampleRate);
            nodes.back()->setEnabled (true);
            nodes.back()->setSpeedRatio (0.25);
            nodes.back()->flushBuffers();
            nodes.back()->getNextAudioBlock (juce::AudioSourceChannelInfo (out));
        }

        const auto after = residentBytes();
        if (before > 0.0)
            report << numVoices << " stereo voices: " << (after - before) / (1024.0 * 1024.0) << " MB resident ("
                   << (after - before) / (1024.0 * 1024.0 * numVoices) << " MB per voice)\n";
    }

    WARN (report.str());
}
