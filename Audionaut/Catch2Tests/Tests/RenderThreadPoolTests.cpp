//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <catch2/catch_test_macros.hpp>

#include <atomic>

#include "Engine/Playback/RenderThreadPool.h"
#include "TestUtils.h"

using namespace audium;

namespace
{
    constexpr int kBlockSize = 512;
    constexpr double kSampleRate = 48000.0;
    constexpr double kBlockSeconds = kBlockSize / kSampleRate;   // 10.67 ms

    /// A job that keeps its thread busy for jobSeconds and notes who ran it.
    struct BusyJobs
    {
        double jobSeconds = 0.0;
        std::atomic<int> runs { 0 };
        std::atomic<int> slotsUsed[RenderThreadPool::maxThreadSlots] = {};

        void operator() (int, int workerId) noexcept
        {
            const auto until = juce::Time::getHighResolutionTicks()
                             + static_cast<juce::int64> (jobSeconds * static_cast<double> (juce::Time::getHighResolutionTicksPerSecond()));
            while (juce::Time::getHighResolutionTicks() < until)
                ;
            runs.fetch_add (1);
            slotsUsed[workerId].fetch_add (1);
        }
    };

    float sumThreadLoad (const RenderThreadPool& pool)
    {
        auto sum = 0.0f;
        for (auto slot = 0; slot < pool.getNumThreadSlots(); ++slot)
            sum += pool.getThreadLoad (slot);
        return sum;
    }
}

SCENARIO ("the render pool measures each thread's share of the block", "[engine][playback][parallel][load-meter]")
{
    GIVEN ("four jobs of a tenth of a block each")
    {
        BusyJobs jobs;
        jobs.jobSeconds = 0.1 * kBlockSeconds;
        const auto expectedWork = 0.4f;   // four jobs, summed over the threads that ran them

        WHEN ("a pool with three workers runs them for many blocks")
        {
            RenderThreadPool pool;
            pool.start (3, kBlockSize, kSampleRate);
            REQUIRE (pool.getNumThreadSlots() == 4);

            for (auto block = 0; block < 60; ++block)
                pool.run (4, jobs);

            THEN ("every job ran, on the calling thread or a worker")
            {
                REQUIRE (jobs.runs.load() == 240);
                auto onWorkers = 0;
                for (auto slot = 1; slot <= 3; ++slot)
                    onWorkers += jobs.slotsUsed[slot].load();
                REQUIRE (onWorkers > 0);
                REQUIRE (jobs.slotsUsed[0].load() + onWorkers == 240);
            }

            THEN ("the thread loads add up to the work, wherever it ran")
            {
                // the jobs' own time is the floor; scheduling may stretch it
                const auto sum = sumThreadLoad (pool);
                REQUIRE (sum >= expectedWork * 0.95f);

                for (auto slot = 0; slot < 4; ++slot)
                    REQUIRE (pool.getThreadLoad (slot) >= 0.0f);
                REQUIRE (pool.getThreadLoad (RenderThreadPool::maxThreadSlots) == 0.0f);

                if (everyThreadSlotHasACore (pool))
                {
                    REQUIRE (sum <= expectedWork * 2.5f);
                    for (auto slot = 0; slot < 4; ++slot)
                        REQUIRE (pool.getThreadLoad (slot) <= 1.0f);
                }
            }

            THEN ("the calling thread's wait never counts as its own work")
            {
                REQUIRE (pool.getSpinLoad() >= 0.0f);

                // the wait is measured apart from the jobs; whether it was
                // counted shows in the calling thread's own load, which is
                // only bounded where the pool's timing is
                if (everyThreadSlotHasACore (pool))
                {
                    REQUIRE (pool.getSpinLoad() <= 1.0f);
                    REQUIRE (pool.getThreadLoad (0) + pool.getSpinLoad() <= 1.0f);
                }
            }

            pool.stop();
        }

        WHEN ("a pool without workers runs them")
        {
            RenderThreadPool pool;
            pool.start (0, kBlockSize, kSampleRate);
            REQUIRE (pool.getNumThreadSlots() == 1);

            for (auto block = 0; block < 60; ++block)
                pool.run (4, jobs);

            THEN ("the whole work is the calling thread's and nothing was waited for")
            {
                REQUIRE (jobs.slotsUsed[0].load() == 240);
                REQUIRE (pool.getThreadLoad (0) >= expectedWork * 0.95f);
                REQUIRE (pool.getThreadLoad (1) == 0.0f);
                REQUIRE (pool.getSpinLoad() == 0.0f);

                if (everyThreadSlotHasACore (pool))
                    REQUIRE (pool.getThreadLoad (0) <= expectedWork * 2.5f);
            }
        }

        WHEN ("a pool is started again")
        {
            RenderThreadPool pool;
            pool.start (2, kBlockSize, kSampleRate);
            for (auto block = 0; block < 20; ++block)
                pool.run (4, jobs);
            REQUIRE (sumThreadLoad (pool) > 0.0f);

            pool.start (2, kBlockSize, kSampleRate);

            THEN ("its loads start from zero")
            {
                REQUIRE (sumThreadLoad (pool) == 0.0f);
                REQUIRE (pool.getSpinLoad() == 0.0f);
            }
        }
    }
}

SCENARIO ("the render pool stops before its workers have woken up", "[engine][playback][parallel]")
{
    // stop() sets each worker's exit flag and bumps its wake counter. A
    // worker that had not reached its first wait when that happened must
    // not read the bumped counter as the value to wait for: it would then
    // sleep on a counter nobody bumps again, and stop() would hang in the
    // join. The device stopping right after it started is exactly this
    // (ParallelRenderTests does it on purpose), and Windows CI hung there.
    // How often the race is hit depends on how fast a thread starts, so
    // this goes round a number of times.
    GIVEN ("a pool started and stopped again at once, many times over")
    {
        RenderThreadPool pool;
        const auto startMs = juce::Time::getMillisecondCounterHiRes();

        for (auto round = 0; round < 50; ++round)
        {
            pool.start (3, kBlockSize, kSampleRate);
            pool.stop();
        }

        THEN ("every stop joins its workers")
        {
            REQUIRE_FALSE (pool.isRunning());
            REQUIRE (pool.getNumWorkers() == 0);
            // the rounds end: 50 thread start-ups and joins take well under
            // a second, a lost exit signal takes forever
            REQUIRE (juce::Time::getMillisecondCounterHiRes() - startMs < 30000.0);
        }
    }
}
