#include <algorithm>

#include <catch2/catch_test_macros.hpp>

#include "Engine/Analysis/AnalysisProvider.h"
#include "Engine/Analysis/AnalysisWorker.h"

using namespace audium;

SCENARIO("AnalysisWorker cancels queued analyses", "[engine][analysis][worker]")
{
    auto testFilesDirectory = String(CURRENT_SOURCE_DIR) + String("/TestFiles/");
    auto audioFile = File(testFilesDirectory + "silence-fade.aiff");
    REQUIRE(audioFile.existsAsFile());

    auto otherFile = File(testFilesDirectory + "_export_TRK-18.wav");
    REQUIRE(otherFile.existsAsFile());

    // A null provider makes each job a no-op, so the queue handling can be
    // exercised without running (slow) Essentia analyses.
    AnalysisWorker worker(nullptr);

    WHEN("jobs for a file are enqueued and cancelled")
    {
        worker.enqueue(audioFile);
        worker.cancel(audioFile);

        THEN("no job for the file is pending anymore")
        {
            REQUIRE(worker.getPendingCount() == 0);
        }
    }

    WHEN("all jobs are cancelled")
    {
        worker.enqueue(audioFile);
        worker.enqueue(otherFile);
        worker.cancelAll();

        THEN("nothing is pending anymore")
        {
            REQUIRE(worker.getPendingCount() == 0);
        }
    }

    WHEN("a file that was never enqueued is cancelled")
    {
        worker.cancel(audioFile);

        THEN("the worker is unaffected")
        {
            REQUIRE(worker.getPendingCount() == 0);
        }
    }
}

SCENARIO("AnalysisWorker runs the merge's analyses first", "[engine][analysis][worker]")
{
    // Jobs run in the order they are queued, so whatever Auto Edit needs should
    // be at the front: it can then run without waiting for the analyses only
    // the waveform display uses.
    AnalysisWorker worker(nullptr);

    const auto& defaults = worker.getDefaultAnalysisTypes();
    const auto& mergeTypes = AnalysisProvider::getMergeAnalysisTypes();

    GIVEN("the default analysis order")
    {
        THEN("every analysis the merge needs is queued")
        {
            for (auto mergeType : mergeTypes)
                REQUIRE(std::find(defaults.begin(), defaults.end(), mergeType) != defaults.end());
        }

        THEN("they lead the queue, ahead of the others")
        {
            REQUIRE(defaults.size() >= mergeTypes.size());

            for (size_t i = 0; i < mergeTypes.size(); ++i)
                REQUIRE(defaults[i] == mergeTypes[i]);
        }

        THEN("the remaining analyses still run afterwards")
        {
            REQUIRE(defaults.size() == 4);
        }
    }
}
