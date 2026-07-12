//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "SBicSegmenter.h"

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
#endif

namespace audium {

std::vector<float> SBicSegmenter::analyze(const juce::File& audioFile)
{
    return analyze(audioFile, Parameters());
}

std::vector<float> SBicSegmenter::analyze(const juce::File& audioFile,
                                          const Parameters& params)
{
#if ! ESSENTIA_ENABLED
    juce::ignoreUnused (audioFile, params);
    return {};
#else
    if (! audioFile.existsAsFile())
        return {};

    using namespace essentia;
    using namespace essentia::standard;

    const auto audioFilename = audioFile.getFullPathName().toStdString();

    // register the algorithms in the factory(ies)
    essentia::init();

    AlgorithmFactory& factory = standard::AlgorithmFactory::instance();

    // MonoLoader -> FrameCutter -> Windowing -> Spectrum -> MFCC -> SBic
    auto audio = std::unique_ptr<Algorithm>(factory.create("MonoLoader",
                                                           "filename", audioFilename,
                                                           "sampleRate", (Real) params.sampleRate));

    auto fc = std::unique_ptr<Algorithm>(factory.create("FrameCutter",
                                                        "frameSize", params.frameSize,
                                                        "hopSize", params.hopSize));

    auto w = std::unique_ptr<Algorithm>(factory.create("Windowing",
                                                       "type", "blackmanharris62"));

    auto spec = std::unique_ptr<Algorithm>(factory.create("Spectrum"));

    auto mfcc = std::unique_ptr<Algorithm>(factory.create("MFCC"));

    auto sbic = std::unique_ptr<Algorithm>(factory.create("SBic",
                                                          "size1", params.size1,
                                                          "inc1", params.inc1,
                                                          "size2", params.size2,
                                                          "inc2", params.inc2,
                                                          "cpw", (Real) params.complexityPenaltyWeight,
                                                          "minLength", params.minimumSegmentLength));

    // Audio -> FrameCutter
    std::vector<Real> audioBuffer;
    audio->output("audio").set(audioBuffer);
    fc->input("signal").set(audioBuffer);

    // FrameCutter -> Windowing -> Spectrum
    std::vector<Real> frameVector, windowedFrameVector;
    fc->output("frame").set(frameVector);
    w->input("frame").set(frameVector);
    w->output("frame").set(windowedFrameVector);
    spec->input("frame").set(windowedFrameVector);

    // Spectrum -> MFCC
    std::vector<Real> spectrum, mfccCoeffs, mfccBands;
    spec->output("spectrum").set(spectrum);
    mfcc->input("spectrum").set(spectrum);
    mfcc->output("bands").set(mfccBands);
    mfcc->output("mfcc").set(mfccCoeffs);

    audio->compute();

    Pool pool;
    while (true)
    {
        fc->compute();

        // the last (empty) frame signals we're done
        if (frameVector.empty())
            break;

        w->compute();
        spec->compute();
        mfcc->compute();

        pool.add("lowlevel.mfcc", mfccCoeffs);
    }

    std::vector<std::vector<Real>> features;
    try
    {
        features = pool.value<std::vector<std::vector<Real>>>("lowlevel.mfcc");
    }
    catch (const EssentiaException&)
    {
        essentia::shutdown();
        return {};
    }

    if (features.empty())
    {
        essentia::shutdown();
        return {};
    }

    // Essentia SBic expects features laid out as [coefficient][frame]
    TNT::Array2D<Real> featuresArray((int) features[0].size(), (int) features.size());
    for (auto frameIdx = 0; frameIdx < (int) features.size(); ++frameIdx)
        for (auto mfccIdx = 0; mfccIdx < (int) features[0].size(); ++mfccIdx)
            featuresArray[mfccIdx][frameIdx] = features[(size_t) frameIdx][(size_t) mfccIdx];

    std::vector<Real> segments;
    sbic->input("features").set(featuresArray);
    sbic->output("segmentation").set(segments);
    sbic->compute();

    // convert frame indices to seconds
    std::vector<float> timestamps;
    timestamps.reserve(segments.size());
    for (auto seg : segments)
        timestamps.push_back((float) (seg * Real(params.hopSize) / (Real) params.sampleRate));

    essentia::shutdown();

    return timestamps;
#endif // ESSENTIA_ENABLED
}

} // namespace audium
