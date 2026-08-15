#include <catch2/catch_test_macros.hpp>

// This whole translation unit exercises Essentia directly, so it is compiled
// out when the codebase is built without Essentia. See ESSENTIA_ENABLED in the
// segmenters.
#ifndef ESSENTIA_ENABLED
 #if __has_include(<essentia/algorithmfactory.h>) && __has_include(<unsupported/Eigen/CXX11/Tensor>)
  #define ESSENTIA_ENABLED 1
 #else
  #define ESSENTIA_ENABLED 0
 #endif
#endif

#if ESSENTIA_ENABLED

#define EIGEN_HAS_STD_RESULT_OF 0

#include <iostream>
#include <fstream>

#include <essentia/algorithmfactory.h>
#include <essentia/pool.h>
#include <essentia/scheduler/network.h>
#include <essentia/streaming/algorithms/poolstorage.h>

#include <nlohmann/json.hpp>

#include "Engine/Factory/AudiumFactory.h"

using json = nlohmann::json;

using namespace std;
using namespace essentia;
using namespace standard;


using namespace audium;

SCENARIO("essentia segmentation test", "[engine][essentia][segmentation]")
{

    auto testFilesDirectory = String(CURRENT_SOURCE_DIR) + String("/TestFiles/");
    auto inFile = File(testFilesDirectory + "_export_TRK-18.wav");
    REQUIRE(inFile.existsAsFile());
    string audioFilename = inFile.getFullPathName().toStdString();
    auto outFile = File(testFilesDirectory + "segmentation-result.json");


    // register the algorithms in the factory(ies)
    essentia::init();



    /////// PARAMS //////////////
    Real sampleRate = 44100.0;
    int frameSize = 2048;
    int hopSize = 1024;

    // BIC segmentation parameters in frames
    auto minimumSegmentsLength = 10;
    auto size1 = 300, inc1 = 60, size2 = 200, inc2 = 20;

    // complexity penalty weight [0, ∞]
    auto cpw = 1.5;

    // we want to compute the MFCC of a file: we need the create the following:
    // audioloader -> framecutter -> windowing -> FFT -> MFCC

    AlgorithmFactory& factory = standard::AlgorithmFactory::instance();


    auto audio = std::unique_ptr<Algorithm>(factory.create("MonoLoader",
                                                          "filename", audioFilename,
                                                          "sampleRate", sampleRate));

    auto fc = std::unique_ptr<Algorithm>(factory.create("FrameCutter",
                                                        "frameSize", frameSize,
                                                        "hopSize", hopSize));

    auto w = std::unique_ptr<Algorithm>(factory.create("Windowing",
                                                       "type", "blackmanharris62"));

    auto spec = std::unique_ptr<Algorithm>(factory.create("Spectrum"));

    auto mfcc = std::unique_ptr<Algorithm>(factory.create("MFCC"));

    auto sbic = std::unique_ptr<Algorithm>(factory.create("SBic",
                                                          "size1", size1,
                                                          "inc1", inc1,
                                                          "size2", size2,
                                                          "inc2", inc2,
                                                          "cpw", cpw,
                                                          "minLength", minimumSegmentsLength));

    /////////// CONNECTING THE ALGORITHMS ////////////////
    cout << "-------- connecting algos ---------" << endl;

    // Audio -> FrameCutter
    vector<Real> audioBuffer;

    audio->output("audio").set(audioBuffer);

    // TODO: try to use juce audio reader
    fc->input("signal").set(audioBuffer);

    // FrameCutter -> Windowing -> Spectrum
    vector<Real> frameVector, windowedFrameVector;

    fc->output("frame").set(frameVector);
    w->input("frame").set(frameVector);

    w->output("frame").set(windowedFrameVector);
    spec->input("frame").set(windowedFrameVector);

    // Spectrum -> MFCC
    vector<Real> spectrum, mfccCoeffs, mfccBands;

    spec->output("spectrum").set(spectrum);
    mfcc->input("spectrum").set(spectrum);

    mfcc->output("bands").set(mfccBands);
    mfcc->output("mfcc").set(mfccCoeffs);


    /////////// STARTING THE ALGORITHMS //////////////////
    cout << "-------- start processing " << audioFilename << " --------" << endl;

    audio->compute();

    std::cout << audioBuffer.size() << std::endl;

    Pool pool;

    while (true) {

      // compute a frame
      fc->compute();

      // if it was the last one (ie: it was empty), then we're done.
      if (!frameVector.size()) {
        break;
      }

      // if the frame is silent, just drop it and go on processing
//      if (isSilent(frame)) continue;

      w->compute();
      spec->compute();
      mfcc->compute();

      pool.add("lowlevel.mfcc", mfccCoeffs);

    }


    vector<vector<Real>> features;
    try {
      features = pool.value<vector<vector<Real> > >("lowlevel.mfcc");
    }
    catch(const EssentiaException&) {
      cerr << "Error: could not find MFCC features in low level pool. Aborting..." << endl;
      exit(3);
    }

    TNT::Array2D<Real> featuresArray((int)features[0].size(), (int)features.size());
    for (auto frameIdx = 0; frameIdx < int(features.size()); ++frameIdx) {
      for (auto mfccIdx = 0; mfccIdx < int(features[0].size()); ++mfccIdx) {
        featuresArray[mfccIdx][frameIdx] = features[(size_t)frameIdx][(size_t)mfccIdx];
      }
    }
    // BIC segmentation
    vector<Real> segments;
    sbic->input("features").set(featuresArray);
    sbic->output("segmentation").set(segments);
    sbic->compute();

    Pool eqPool;
    for (size_t i = 0; i < segments.size(); ++i) {
        segments[i] *= Real(hopSize) / sampleRate;
        std::cout << "segment " << i << ": " << segments[i] << " seconds" << std::endl;
        eqPool.add("segmentation.timestamps", segments[i]);
    }


    // write results to file
    cout << "-------- writing results to file " << outFile.getFullPathName() << " ---------" << endl;
    if (outFile.existsAsFile()) outFile.deleteFile();

    auto outStream = std::unique_ptr<juce::FileOutputStream>(outFile.createOutputStream());
    json j;
    j["segmentation.timestamps"] = segments;
    outStream->writeString(j.dump(4));

    cout << j.dump(4) << endl;

    // cleanup
    if (outFile.existsAsFile()) outFile.deleteFile();


    essentia::shutdown();
}

SCENARIO("essentia onsets test", "[engine][essentia][onsets]")
{
    auto testFilesDirectory = String(CURRENT_SOURCE_DIR) + String("/TestFiles/");
    auto inFile = File(testFilesDirectory + "_export_TRK-18.wav");
    REQUIRE(inFile.existsAsFile());
    string audioFilename = inFile.getFullPathName().toStdString();
    auto outFile = File(testFilesDirectory + "onset-result.json");


    // register the algorithms in the factory(ies)
    essentia::init();

    Real onsetRate;
    vector<Real> onsets;

    vector<Real> audio, unused;

    // File Input
    Algorithm* audiofile = AlgorithmFactory::create("MonoLoader",
                                                    "filename", audioFilename,
                                                    "sampleRate", 44100);

    Algorithm* extractoronsetrate = AlgorithmFactory::create("OnsetRate");

    audiofile->output("audio").set(audio);

    extractoronsetrate->input("signal").set(audio);
    extractoronsetrate->output("onsets").set(onsets);
    extractoronsetrate->output("onsetRate").set(onsetRate);

    audiofile->compute();
    extractoronsetrate->compute();

    cout << "onsetRate: " << onsetRate << endl;


    // write results to file
    cout << "-------- writing results to file " << outFile.getFullPathName() << " ---------" << endl;
    if (outFile.existsAsFile()) outFile.deleteFile();

    auto outStream = std::unique_ptr<juce::FileOutputStream>(outFile.createOutputStream());
    json j;
    j["onset.timestamps"] = onsets;
    outStream->writeString(j.dump(4));

//    cout << j.dump(4) << endl;

    // cleanup
    if (outFile.existsAsFile()) outFile.deleteFile();

    delete extractoronsetrate;
    delete audiofile;

    essentia::shutdown();

}

#endif // ESSENTIA_ENABLED
