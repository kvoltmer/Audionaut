//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "BeatSegmenter.h"

// ESSENTIA_ENABLED lets the codebase build without the (prebuilt) Essentia
// library present. When it is 0 the analysis is compiled out and analyze()
// returns an empty result. Defaults to on so normal builds are unaffected.
#ifndef ESSENTIA_ENABLED
 #define ESSENTIA_ENABLED 1
#endif

#if ESSENTIA_ENABLED
 #define EIGEN_HAS_STD_RESULT_OF 0
 #include <essentia/algorithmfactory.h>
 #include <essentia/pool.h>
 #include <essentia/scheduler/network.h>
 #include <essentia/streaming/algorithms/poolstorage.h>
#endif

namespace audium {

std::vector<float> BeatSegmenter::analyze(const juce::File& audioFile)
{
    return analyze(audioFile, Parameters());
}

std::vector<float> BeatSegmenter::analyze(const juce::File& audioFile,
                                          const Parameters& params)
{
#if ! ESSENTIA_ENABLED
    juce::ignoreUnused (audioFile, params);
    return {};
#else
    if (! audioFile.existsAsFile())
        return {};

    using namespace essentia;
    using namespace essentia::streaming;
    using namespace essentia::scheduler;

    const auto audioFilename = audioFile.getFullPathName().toStdString();

    // register the algorithms in the factory(ies)
    essentia::init();

    Pool pool;

    streaming::AlgorithmFactory& factory = streaming::AlgorithmFactory::instance();

    // MonoLoader -> BeatTrackerMultiFeature
    streaming::Algorithm* monoloader = factory.create("MonoLoader", "filename", audioFilename);
    streaming::Algorithm* beattracker = factory.create("BeatTrackerMultiFeature");

    monoloader->configure("sampleRate", (Real) params.sampleRate);

    monoloader->output("audio")       >> beattracker->input("signal");
    beattracker->output("ticks")      >> PC(pool, "rhythm.ticks");
    beattracker->output("confidence") >> NOWHERE;

    // The Network takes ownership of the connected algorithms and frees them.
    Network network(monoloader);
    network.run();

    // BeatTrackerMultiFeature reports beat positions in seconds. The pool may
    // be empty when no beats were found.
    std::vector<float> timestamps;
    if (pool.contains<std::vector<Real>>("rhythm.ticks"))
    {
        const auto ticks = pool.value<std::vector<Real>>("rhythm.ticks");
        timestamps.reserve(ticks.size());
        for (auto tick : ticks)
            timestamps.push_back((float) tick);
    }

    essentia::shutdown();

    return timestamps;
#endif // ESSENTIA_ENABLED
}

} // namespace audium
