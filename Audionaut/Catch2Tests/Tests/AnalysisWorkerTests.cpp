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

SCENARIO("AnalysisWorker reports per-file remaining counts", "[engine][analysis][worker]")
{
    auto testFilesDirectory = String(CURRENT_SOURCE_DIR) + String("/TestFiles/");
    auto audioFile = File(testFilesDirectory + "silence-fade.aiff");
    REQUIRE(audioFile.existsAsFile());

    auto otherFile = File(testFilesDirectory + "_export_TRK-18.wav");
    REQUIRE(otherFile.existsAsFile());

    AnalysisWorker worker(nullptr);

    // The null-provider worker drains its queue in the background, so only
    // assertions that hold regardless of how far it has got are made here.
    WHEN("nothing was ever enqueued for a file")
    {
        THEN("its remaining count is zero")
        {
            REQUIRE(worker.getRemainingCount(audioFile) == 0);
        }
    }

    WHEN("enqueued files have been fully processed")
    {
        worker.enqueue(audioFile);
        worker.enqueue(otherFile);

        // Null-provider jobs are no-ops, so the queue drains promptly; poll
        // rather than sleep a fixed time.
        for (int i = 0; i < 500 && worker.getRemainingCount() > 0; ++i)
            Thread::sleep(10);
        REQUIRE(worker.getRemainingCount() == 0);

        THEN("every per-file remaining count is zero again")
        {
            REQUIRE(worker.getRemainingCount(audioFile) == 0);
            REQUIRE(worker.getRemainingCount(otherFile) == 0);
        }
    }
}

SCENARIO("AnalysisWorker runs the merge's analyses first", "[engine][analysis][worker]")
{
    const auto& mergeTypes = AnalysisProvider::getMergeAnalysisTypes();

    GIVEN("a worker with the built-in defaults")
    {
        AnalysisWorker worker(nullptr);

        THEN("exactly the analyses the merge needs are queued automatically")
        {
            // Out of the box only Auto Edit's analyses run; the display-only
            // ones are opt-in via the settings.
            REQUIRE(worker.getDefaultAnalysisTypes() == mergeTypes);
        }
    }

    GIVEN("a worker constructed with every analysis type")
    {
        // Jobs run in the order they are queued, so whatever Auto Edit needs
        // should be at the front: it can then run without waiting for the
        // analyses only the waveform display uses.
        AnalysisWorker worker(nullptr, AnalysisWorker::canonicalAnalysisTypes());

        const auto defaults = worker.getDefaultAnalysisTypes();

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

SCENARIO("AnalysisWorker's automatic analysis can be configured", "[engine][analysis][worker]")
{
    auto testFilesDirectory = String(CURRENT_SOURCE_DIR) + String("/TestFiles/");
    auto audioFile = File(testFilesDirectory + "silence-fade.aiff");
    REQUIRE(audioFile.existsAsFile());

    AnalysisWorker worker(nullptr, AnalysisWorker::canonicalAnalysisTypes());

    WHEN("automatic analysis is disabled")
    {
        worker.setAutoAnalysisEnabled(false);

        THEN("enqueueing with the defaults queues nothing")
        {
            REQUIRE_FALSE(worker.isAutoAnalysisEnabled());
            REQUIRE(worker.enqueue(audioFile) == 0);
        }

        THEN("an explicitly requested analysis still queues")
        {
            REQUIRE(worker.enqueue(audioFile, { AnalysisType::Onset }) == 1);
        }

        THEN("re-enabling restores the default behaviour")
        {
            worker.setAutoAnalysisEnabled(true);
            REQUIRE(worker.enqueue(audioFile) == 4);
        }
    }

    WHEN("the default types are replaced in an arbitrary order")
    {
        worker.setDefaultAnalysisTypes({ AnalysisType::Beat, AnalysisType::Onset,
                                         AnalysisType::SBic, AnalysisType::BeatDegara });

        THEN("they are stored in canonical, merge-first order")
        {
            REQUIRE(worker.getDefaultAnalysisTypes() == AnalysisWorker::canonicalAnalysisTypes());
        }
    }

    WHEN("the default types are replaced with a subset")
    {
        worker.setDefaultAnalysisTypes({ AnalysisType::Onset, AnalysisType::SBic });

        THEN("the canonical order is kept within the subset")
        {
            const std::vector<AnalysisType> expected { AnalysisType::SBic, AnalysisType::Onset };
            REQUIRE(worker.getDefaultAnalysisTypes() == expected);
        }
    }

    WHEN("the default types are cleared")
    {
        worker.setDefaultAnalysisTypes({});

        THEN("enqueueing with the defaults queues nothing")
        {
            REQUIRE(worker.enqueue(audioFile) == 0);
        }

        THEN("an explicitly requested analysis still queues")
        {
            REQUIRE(worker.enqueue(audioFile, { AnalysisType::Beat }) == 1);
        }
    }
}
