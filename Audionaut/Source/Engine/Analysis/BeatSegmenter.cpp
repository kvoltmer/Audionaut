//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "BeatSegmenter.h"

// ESSENTIA_ENABLED lets the codebase build without the (prebuilt) Essentia
// library present. When it is 0 the analysis is compiled out and analyze()
// returns an empty result. It is auto-detected from the availability of the
// Essentia and Eigen headers (Eigen lives under Essentia's 3rd-party include
// tree, which is a build artifact absent from a clean Essentia checkout), and
// can be forced by the build system defining it explicitly.
#ifndef ESSENTIA_ENABLED
 #if __has_include(<essentia/algorithmfactory.h>) && __has_include(<unsupported/Eigen/CXX11/Tensor>)
  #define ESSENTIA_ENABLED 1
 #else
  #define ESSENTIA_ENABLED 0
 #endif
#endif

#if ESSENTIA_ENABLED
 #define EIGEN_HAS_STD_RESULT_OF 0
 #include <essentia/algorithmfactory.h>
 #include <essentia/pool.h>
 #include <essentia/scheduler/network.h>
 #include <essentia/streaming/algorithms/poolstorage.h>
#endif

namespace audium {

BeatSegmenter::Result BeatSegmenter::analyze(const juce::File& audioFile)
{
    return analyze(audioFile, Parameters());
}

BeatSegmenter::Result BeatSegmenter::analyze(const juce::File& audioFile,
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

    const auto methodString = (params.method == Method::Degara) ? "degara"
                                                                  : "multifeature";

    // MonoLoader -> RhythmExtractor2013 (method=multifeature/degara)
    streaming::Algorithm* monoloader = factory.create("MonoLoader", "filename", audioFilename);
    streaming::Algorithm* beattracker = factory.create("RhythmExtractor2013",
                                                        "method", methodString,
                                                        "maxTempo", params.maxTempo,
                                                        "minTempo", params.minTempo);

    monoloader->configure("sampleRate", (Real) params.sampleRate);

    monoloader->output("audio")        >> beattracker->input("signal");
    beattracker->output("ticks")       >> PC(pool, "rhythm.ticks");
    beattracker->output("confidence")  >> NOWHERE;
    beattracker->output("bpm")         >> PC(pool, "rhythm.bpm");
    beattracker->output("estimates")   >> NOWHERE;
    beattracker->output("bpmIntervals") >> NOWHERE;

    // The Network takes ownership of the connected algorithms and frees them.
    Network network(monoloader);
    network.run();

    // BeatTrackerMultiFeature reports beat positions in seconds. The pool may
    // be empty when no beats were found.
    Result result;
    if (pool.contains<std::vector<Real>>("rhythm.ticks"))
    {
        const auto ticks = pool.value<std::vector<Real>>("rhythm.ticks");
        result.beats.reserve(ticks.size());
        for (auto tick : ticks)
            result.beats.push_back((float) tick);
    }

    if (pool.contains<Real>("rhythm.bpm"))
        result.bpm = (float) pool.value<Real>("rhythm.bpm");

    essentia::shutdown();

    return result;
#endif // ESSENTIA_ENABLED
}

} // namespace audium
