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

using namespace audium;

using namespace essentia::streaming;
using namespace essentia::scheduler;

SCENARIO("essentia multi-feature", "[engine][essentia][BeatTrackerMultiFeature]")
{
    auto testFilesDirectory = String(CURRENT_SOURCE_DIR) + String("/TestFiles/");
    auto inFile = File(testFilesDirectory + "_export_TRK-18.wav");
    REQUIRE(inFile.existsAsFile());
    string audioFilename = inFile.getFullPathName().toStdString();
    auto outFile = File(testFilesDirectory + "beattracker-result.json");

    // register the algorithms in the factory(ies)
    essentia::init();

    Pool pool;

    cout << "Multifeature beat tracker based on BeatTrackerMultiFeature algorithm." << endl;
    cout << "Outputs beat positions in MIREX 2013 format." << endl;


    string outputFilename = outFile.getFullPathName().toStdString();

    // NOTE: this is the streaming::AlgorithmFactory namespace
    streaming::AlgorithmFactory& factory = streaming::AlgorithmFactory::instance();

    streaming::Algorithm* monoloader = factory.create("MonoLoader", "filename", audioFilename);

    streaming::Algorithm* beattracker = factory.create("BeatTrackerMultiFeature");

    monoloader->configure("sampleRate", 44100.);

    /////////// CONNECTING THE ALGORITHMS ////////////////
    cout << "-------- connecting algos --------" << endl;


// NOTE:
// #define PC essentia::streaming::PoolConnector -> inline void operator>>


    monoloader->output("audio") >> beattracker->input("signal");
    beattracker->output("ticks")      >> PC(pool, "rhythm.ticks");
    beattracker->output("confidence") >> NOWHERE;

    /////////// STARTING THE ALGORITHMS //////////////////
    cout << "-------- start processing " << audioFilename << " --------" << endl;

    essentia::scheduler::Network network(monoloader);
    network.run();


    // writing results to file
    vector<Real> ticks;
    if (pool.contains<vector<Real> >("rhythm.ticks")) { // there might be empty ticks
      ticks = pool.value<vector<Real> >("rhythm.ticks");
    }

    // write results to file
    cout << "-------- writing results to file " << outFile.getFullPathName() << " ---------" << endl;
    if (outFile.existsAsFile()) outFile.deleteFile();

    auto outStream = std::unique_ptr<juce::FileOutputStream>(outFile.createOutputStream());
    json j;
    j["rhythm.ticks"] = ticks;
    outStream->writeString(j.dump(4));

//    cout << j.dump(4) << endl;

    // cleanup
    if (outFile.existsAsFile()) outFile.deleteFile();

    essentia::shutdown();
}

#endif // ESSENTIA_ENABLED
