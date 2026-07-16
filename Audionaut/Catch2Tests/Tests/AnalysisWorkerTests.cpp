#include <catch2/catch_test_macros.hpp>

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
