//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "OnsetSegmenter.h"

#define EIGEN_HAS_STD_RESULT_OF 0

#include <essentia/algorithmfactory.h>

namespace audium {

std::vector<float> OnsetSegmenter::analyze(const juce::File& audioFile)
{
    return analyze(audioFile, Parameters());
}

std::vector<float> OnsetSegmenter::analyze(const juce::File& audioFile,
                                           const Parameters& params)
{
    if (! audioFile.existsAsFile())
        return {};

    using namespace essentia;
    using namespace essentia::standard;

    const auto audioFilename = audioFile.getFullPathName().toStdString();

    // register the algorithms in the factory(ies)
    essentia::init();

    AlgorithmFactory& factory = standard::AlgorithmFactory::instance();

    // MonoLoader -> OnsetRate
    auto audio = std::unique_ptr<Algorithm>(factory.create("MonoLoader",
                                                           "filename", audioFilename,
                                                           "sampleRate", (Real) params.sampleRate));

    auto onsetRate = std::unique_ptr<Algorithm>(factory.create("OnsetRate"));

    std::vector<Real> audioBuffer;
    audio->output("audio").set(audioBuffer);

    std::vector<Real> onsets;
    Real rate;
    onsetRate->input("signal").set(audioBuffer);
    onsetRate->output("onsets").set(onsets);
    onsetRate->output("onsetRate").set(rate);

    audio->compute();
    onsetRate->compute();

    // OnsetRate already reports onset positions in seconds.
    std::vector<float> timestamps;
    timestamps.reserve(onsets.size());
    for (auto onset : onsets)
        timestamps.push_back((float) onset);

    essentia::shutdown();

    return timestamps;
}

} // namespace audium
