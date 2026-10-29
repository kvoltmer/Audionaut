//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

#include "RenderLoadMeter.h"

namespace audium
{

/**
 * Real-time worker threads that share the audio callback's work.
 *
 * Lifecycle:
 *  - started by the device's about-to-start callback with the device's block
 *    size and (on macOS) its audio workgroup, stopped when the device stops;
 *    never from the audio thread
 *  - fed by the audio callback: run() hands the pool a stage of independent
 *    jobs and returns once every one is done
 *  - the calling thread takes jobs itself (as worker 0), the workers take
 *    the rest, so with no workers the pool is the serial path through the
 *    same code
 *  - workers sleep on an event between stages
 *
 * Nothing in run() allocates or blocks on a lock. Jobs are handed out by one
 * atomic ticket counter, completion is counted down by another, and the
 * calling thread spins for the last jobs. The workers start as real-time
 * threads inside the device's audio workgroup, so the scheduler treats them
 * as part of the audio deadline.
 *
 * How a stage is handed out, and why it is safe:
 *  1. A ticket names its stage: it is (generation, job index), drawn with one
 *     fetch_add. The stage for a generation lives in stages[generation & 1].
 *  2. A stage entry is overwritten only after every thread has left
 *     takeJobs() for the previous one (participants == 0), and only the
 *     entry of the generation after that - never the one a late reader may
 *     still hold.
 *  3. A worker that wakes after its stage is finished therefore reads an
 *     intact job count, finds its ticket past the end, and leaves.
 *  4. Ordering: a worker's writes into the audio buffers happen before its
 *     jobsRemaining.fetch_sub (acq_rel), and the calling thread's acquire
 *     load of jobsRemaining / participants in the drain loop makes them
 *     visible before run() returns. The stage entry and the ticket counter
 *     are published with a release store and read with acquire on the
 *     fetch_add. Everything else is relaxed bookkeeping.
 *
 * A stage whose jobs a worker cannot finish (the thread pre-empted mid-job)
 * makes the calling thread wait: it cannot abandon a job whose worker may
 * still write into the buffers. A wait longer than two block periods is
 * counted as a stall (getStallCount), next to the DSP load meter's overrun.
 */
class RenderThreadPool
{
public:
    /// Thread slots with a load reading: the calling thread and every worker.
    static constexpr int maxThreadSlots = RenderLoadMeter::maxThreadSlots;

    /// The most workers a pool runs.
    static constexpr int maxWorkers = maxThreadSlots - 1;

    /// Renders one job. workerId is 0 on the calling thread, 1..numWorkers on a pool thread.
    using JobFunction = void (*) (void* context, int jobIndex, int workerId) noexcept;

    // both out of line: Worker is complete only in the .cpp, and a defaulted
    // constructor would instantiate the workers' destructor for unwinding
    RenderThreadPool();
    ~RenderThreadPool();

    /// Physical cores minus one for the message thread and disk reads, at most maxWorkers.
    static int defaultNumWorkers();

    /**
     * Starts numWorkers threads for a device running blockSize samples at
     * sampleRate (sizes the real-time scheduling and the stall threshold).
     * Stops a running pool first. Not from the audio thread, and not while a
     * callback may be inside run().
     */
    void start (int numWorkers, int blockSize, double sampleRate, juce::AudioWorkgroup workgroup = {});

    /// Joins the workers. Same conditions as start().
    void stop();

    bool isRunning() const noexcept { return ! workers.empty(); }
    int getNumWorkers() const noexcept { return static_cast<int> (workers.size()); }

    /**
     * Audio thread: runs fn (context, job, workerId) for every job in
     * [0, numJobs) across this thread and the workers, returning when all
     * are done. Serial on this thread when the pool has no workers or the
     * stage has one job.
     */
    void run (int numJobs, JobFunction fn, void* context) noexcept;

    /// run() with any callable taking (int jobIndex, int workerId); nothing is copied or allocated.
    template <typename Callable>
    void run (int numJobs, Callable& callable) noexcept
    {
        run (numJobs,
             [] (void* c, int job, int worker) noexcept { (*static_cast<Callable*> (c)) (job, worker); },
             &callable);
    }

    /// Stages where the calling thread waited longer than two block periods for the workers.
    std::uint32_t getStallCount() const noexcept { return stalls.load (std::memory_order_relaxed); }
    void resetStallCount() noexcept { stalls.store (0, std::memory_order_relaxed); }

    /// Thread slots with a load reading: the calling thread plus the workers.
    int getNumThreadSlots() const noexcept { return getNumWorkers() + 1; }

    /**
     * Smoothed share of the block period a thread slot spent inside jobs;
     * slot 0 is the calling thread's own jobs, 1..numWorkers the workers.
     * 1.0 is the whole period. Message thread, like DspLoadMeter::getLoad.
     */
    float getThreadLoad (int slot) const noexcept { return loads.getLoad (slot); }

    /// Smoothed share of the block period the calling thread spent waiting for the workers.
    float getSpinLoad() const noexcept { return loads.getSpinLoad(); }

private:
    class Worker;

    struct Stage
    {
        JobFunction fn = nullptr;
        void* context = nullptr;
        int numJobs = 0;
    };

    /// What a thread draws from the ticket counter: which stage, and which of its jobs.
    struct Ticket
    {
        std::uint32_t generation;
        std::uint32_t job;

        static std::uint64_t encode (std::uint32_t generation, std::uint32_t job) noexcept
        {
            return (static_cast<std::uint64_t> (generation) << 32) | job;
        }

        static Ticket decode (std::uint64_t packed) noexcept
        {
            return { static_cast<std::uint32_t> (packed >> 32), static_cast<std::uint32_t> (packed & 0xffffffffu) };
        }
    };

    // run(), in order: publish, wake, take jobs on this thread, drain
    void publishStage (int numJobs, JobFunction fn, void* context) noexcept;
    void wakeWorkers (int count) noexcept;
    void takeJobs (int workerId) noexcept;
    juce::int64 waitForStageToDrain() noexcept;   // returns the ticks spent waiting

    // the stage ring and its counters; the class comment has the invariants
    Stage stages[2];
    std::atomic<std::uint64_t> nextTicket { 0 };
    std::atomic<int> jobsRemaining { 0 };
    std::atomic<int> participants { 0 };
    std::uint32_t generation = 0;   // audio thread only

    std::vector<std::unique_ptr<Worker>> workers;
    juce::AudioWorkgroup workgroup;
    juce::int64 stallTicks = 0;
    std::atomic<std::uint32_t> stalls { 0 };
    RenderLoadMeter loads;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RenderThreadPool)
};

} // namespace audium
