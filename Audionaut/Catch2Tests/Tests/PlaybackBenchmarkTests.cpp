//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#include "Cli/CliContext.h"
#include "Cli/Commands/Commands.h"
#include "Engine/Analysis/AnalysisWorker.h"
#include "Engine/Factory/AudiumFactory.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/PlayList/ClipDynamics.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/Project/ProjectFileStore.h"
#include "Engine/Resource/AudioResourceContainer.h"

#include "LiveRenderHarness.h"

// How much of the audio callback's time budget playback takes as the track
// count grows. Hidden ([.]): it prints a table and asserts nothing about
// speed, so it only means something in an optimised build on a quiet
// machine:
//
//   cmake -B build-release -S Audionaut/Catch2Tests -DCMAKE_BUILD_TYPE=Release
//   cmake --build build-release
//   ./build-release/AudionautTests_artefacts/Release/AudionautTests "[playback][bench]"
//
// Each track is a stereo 48 kHz file playing for the whole render, with a
// fade-in, a fade-out and clip gain, so every voice runs the resampler-free
// streaming path plus the dynamics. The second table re-runs the sessions
// with every clip time-stretched (Rubber Band R3), the expensive case.
// Load is the callback's wall time over the audio it produced (what the
// header's DSP load meter shows); 1.0 is a dropout. The files come from a
// warm cache, so this measures DSP, not disk.

using namespace audium;
using namespace audium::test;

namespace {

constexpr double kSampleRate = 48000.0;
constexpr int kNumFiles = 16;          // distinct files, cycled over the tracks
constexpr double kFileSeconds = 24.0;
constexpr double kStartSeconds = 1.0;
constexpr double kRenderSeconds = 20.0;

juce::ArgumentList makeArgs (const juce::String& commandLine)
{
    return juce::ArgumentList ("audionaut-cli", commandLine);
}

/// kNumFiles stereo noise files, 24-bit, written once per run.
std::vector<juce::File> writeSourceFiles (const juce::File& dir)
{
    std::vector<juce::File> files;
    juce::WavAudioFormat wav;
    juce::Random random (4711);
    const auto numSamples = static_cast<int> (kFileSeconds * kSampleRate);
    juce::AudioBuffer<float> buffer (2, numSamples);

    for (auto f = 0; f < kNumFiles; ++f)
    {
        for (auto c = 0; c < 2; ++c)
            for (auto i = 0; i < numSamples; ++i)
                buffer.setSample (c, i, (random.nextFloat() * 2.0f - 1.0f) * 0.1f);

        auto file = dir.getChildFile ("noise-" + juce::String (f) + ".wav");
        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());
        REQUIRE (stream != nullptr);
        std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (stream.get(), kSampleRate, 2, 24, {}, 0));
        REQUIRE (writer != nullptr);
        stream.release();
        REQUIRE (writer->writeFromAudioSampleBuffer (buffer, 0, numSamples));
        files.push_back (file);
    }
    return files;
}

/// A project with numTracks stereo tracks, one clip each.
std::shared_ptr<AudiumEngine> buildSession (const juce::File& dir, const std::vector<juce::File>& files,
                                            int numTracks, bool stretch)
{
    const auto project = dir.getChildFile ("bench-" + juce::String (numTracks) + ".audium");
    project.deleteRecursively();
    cli::CliContext context;
    context.quiet = true;
    REQUIRE (cli::runCreate (makeArgs ("create " + project.getFullPathName() + " --channels 2"), context)
             == cli::exitOk);

    auto engine = AudiumFactory::createAudiumEngine();
    engine->getAudioResourceContainer()->getAnalysisWorker()->setAutoAnalysisEnabled (false);
    REQUIRE (engine->getProjectFileStore()->open (project, nullptr));

    auto container = engine->getAudioTrackContainer();
    const auto before = container->getNumItems();
    for (auto t = 0; t < numTracks; ++t)
    {
        juce::StringArray names { files[(size_t) (t % kNumFiles)].getFullPathName() };
        REQUIRE (container->addAudioFiles (names, 0.0, [] (std::string) {}, false));
    }
    REQUIRE (container->getNumItems() - before == numTracks);

    for (const auto& track : container->getAudioTracks())
        for (const auto& item : track->getPlayListContainer()->getPlayListItems())
        {
            auto& dynamics = item->getDynamics();
            dynamics.setFadeIn (0.5);
            dynamics.setFadeOut (0.5);
            dynamics.setGain (0, 0.8);
            dynamics.setGain (1, 0.7);
            if (stretch)
            {
                item->setStretchMode (StretchMode::Stretch);
                item->setSpeedRatio (0.9);
            }
        }

    engine->getPlayListScheduler()->commitPlayListData();
    return engine;
}

struct LoadStats
{
    double mean = 0.0, p99 = 0.0, max = 0.0;
    int overruns = 0;
    float peak = 0.0f;   ///< output level, to show the tracks were heard
};

LoadStats measure (AudiumEngine& engine, int blockSize)
{
    FakeAudioIODevice device (kSampleRate, blockSize, 2, 2);
    LiveBlockDriver driver (engine, device);
    driver.play (kStartSeconds);
    const auto audio = driver.render (kRenderSeconds);
    driver.stop();

    const auto budgetMs = blockSize * 1000.0 / kSampleRate;
    std::vector<double> loads;
    for (const auto& block : driver.trace.blocks)
        if (block.isPlaying)
            loads.push_back (block.blockWallMs / budgetMs);
    REQUIRE (! loads.empty());

    // the first callbacks after Play prime the voices: kept in max, not in
    // the steady-state mean and p99
    const auto settle = std::min<size_t> (loads.size() / 10, static_cast<size_t> (0.25 * kSampleRate / blockSize));

    LoadStats stats;
    stats.max = *std::max_element (loads.begin(), loads.end());
    for (auto l : loads)
        stats.overruns += l >= 1.0 ? 1 : 0;

    std::vector<double> steady (loads.begin() + (long) settle, loads.end());
    for (auto l : steady)
        stats.mean += l;
    stats.mean /= (double) steady.size();
    std::sort (steady.begin(), steady.end());
    stats.p99 = steady[std::min (steady.size() - 1, static_cast<size_t> (0.99 * (double) steady.size()))];

    for (auto c = 0; c < audio.getNumChannels(); ++c)
        stats.peak = std::max (stats.peak, audio.getMagnitude (c, 0, audio.getNumSamples()));
    return stats;
}

void runTable (const juce::File& dir, const std::vector<juce::File>& files, bool stretch,
               const std::vector<int>& trackCounts)
{
    const std::vector<int> blockSizes { 64, 128, 256, 512 };

    std::ostringstream report;
    report << std::fixed << std::setprecision (1)
           << "\n" << (stretch ? "time-stretched clips (Rubber Band R3, x0.9)" : "plain clips (gain + fades)")
           << ", 48 kHz, load in % of the callback budget: mean / p99 / max (overruns)\n"
           << "tracks";
    for (auto b : blockSizes)
        report << std::setw (32) << (juce::String (b) + " samples").toStdString();
    report << "\n";

    for (auto numTracks : trackCounts)
    {
        auto engine = buildSession (dir, files, numTracks, stretch);
        report << std::setw (6) << numTracks;
        for (auto blockSize : blockSizes)
        {
            const auto s = measure (*engine, blockSize);
            CHECK (s.peak > 0.01f);
            std::ostringstream cell;
            cell << std::fixed << std::setprecision (1)
                 << 100.0 * s.mean << " / " << 100.0 * s.p99 << " / " << 100.0 * s.max
                 << " (" << s.overruns << ")";
            report << std::setw (32) << cell.str();
        }
        report << "\n";
        std::cout << report.str() << std::flush;
        report.str ({});
        engine = nullptr;
    }
}

} // namespace

SCENARIO ("playback cost per track count", "[.][playback][bench]")
{
    juce::MessageManager::getInstance();
    juce::MessageManagerLock mmLock (juce::Thread::getCurrentThread());

    {
        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("audionaut-playback-bench");
        dir.deleteRecursively();
        REQUIRE (dir.createDirectory());
        const auto files = writeSourceFiles (dir);

        std::cout << "\nplayback bench: " << juce::SystemStats::getCpuModel() << ", "
                  << juce::SystemStats::getNumCpus() << " cores, "
                  << kRenderSeconds << " s per cell" << std::endl;

        // 64 stereo tracks fill MAX_AUDIO_CHANNELS (128), the most a project can hold
        runTable (dir, files, false, { 8, 16, 32, 64 });
        runTable (dir, files, true, { 4, 8, 16, 32 });

        dir.deleteRecursively();
    }

    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
}
