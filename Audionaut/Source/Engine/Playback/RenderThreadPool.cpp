//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "RenderThreadPool.h"

#include <algorithm>
#include <thread>

#if JUCE_INTEL
 #include <immintrin.h>
#endif

#if JUCE_WINDOWS
 #include <windows.h> // HANDLE, DWORD, WINAPI for the avrt call below
#endif

namespace audium
{

namespace
{
    /// A spin-wait step: tells the core the loop is waiting on another thread.
    inline void cpuRelax() noexcept
    {
       #if JUCE_INTEL
        _mm_pause();
       #elif JUCE_ARM && JUCE_MSVC
        __yield();
       #elif JUCE_ARM
        __asm__ __volatile__ ("yield");
       #else
        std::this_thread::yield();
       #endif
    }

    /**
     * The calling thread's membership of the MMCSS class the WASAPI device
     * thread runs in (juce::Thread's realtime start only raises the Win32
     * priority), for as long as the object lives: the library stays loaded
     * while the thread is registered, and the registration is reverted when
     * the thread leaves. Nothing on the other platforms.
     */
    struct ProAudioClassMembership
    {
        ProAudioClassMembership()
        {
           #if JUCE_WINDOWS
            using SetCharacteristics = HANDLE (WINAPI*) (LPCWSTR, LPDWORD);
            if (auto setCharacteristics = (SetCharacteristics) avrt.getFunction ("AvSetMmThreadCharacteristicsW"))
            {
                DWORD taskIndex = 0;
                task = setCharacteristics (L"Pro Audio", &taskIndex);
            }
           #endif
        }

        ~ProAudioClassMembership()
        {
           #if JUCE_WINDOWS
            using RevertCharacteristics = BOOL (WINAPI*) (HANDLE);
            if (task != nullptr)
                if (auto revertCharacteristics = (RevertCharacteristics) avrt.getFunction ("AvRevertMmThreadCharacteristics"))
                    revertCharacteristics (task);
           #endif
        }

       #if JUCE_WINDOWS
        juce::DynamicLibrary avrt { "avrt.dll" };
        HANDLE task = nullptr;
       #endif
    };
}

//==============================================================================
class RenderThreadPool::Worker : public juce::Thread
{
public:
    Worker (RenderThreadPool& pool_, int workerId_) :
        juce::Thread ("Audionaut render " + juce::String (workerId_)),
        pool (pool_),
        workerId (workerId_)
    {
    }

    void run() override
    {
        // The token leaves the workgroup when it dies, which has to happen
        // on this thread: it lives in run(), not in the Worker object.
        juce::WorkgroupToken token;
        if (pool.workgroup)
            pool.workgroup.join (token);

        const ProAudioClassMembership proAudioClass;

        // the floating-point state is per thread: the same flush-to-zero
        // the callback runs with, so a job computes the same on any thread
        juce::ScopedNoDenormals noDenormals;

        // wait() returns once the counter differs from the value it was
        // given, so a wake sent before the wait is reached is never lost
        auto seen = wake.load (std::memory_order_acquire);

        for (;;)
        {
            // The exit flag is checked between reading the counter and
            // waiting on it, in the opposite order to stop(), which sets
            // the flag and then bumps the counter: a stop that landed
            // before this read has set the flag by now, and one landing
            // after it bumps the counter past `seen`, so the wait returns
            // at once. Without the check, a stop before a worker's first
            // wait (the device stopped right after it started, as the
            // tests do) was absorbed into `seen`, the worker slept on a
            // counter that never changed again, and stop() hung in the
            // join - which it did on Windows CI, where a thread is slow
            // to start.
            if (threadShouldExit())
                return;

            wake.wait (seen, std::memory_order_acquire);
            seen = wake.load (std::memory_order_acquire);

            if (threadShouldExit())
                return;

            pool.takeJobs (workerId);
        }
    }

    /// Bumped and notified for every stage this worker takes part in, and to stop.
    void signal() noexcept
    {
        wake.fetch_add (1, std::memory_order_release);
        wake.notify_one();
    }

private:
    RenderThreadPool& pool;
    const int workerId;
    std::atomic<std::uint32_t> wake { 0 };   // a counter, waited on with atomic::wait: no mutex on the signal path
};

//==============================================================================
RenderThreadPool::RenderThreadPool() = default;

RenderThreadPool::~RenderThreadPool()
{
    stop();
}

int RenderThreadPool::defaultNumWorkers()
{
    return juce::jlimit (0, maxWorkers, juce::SystemStats::getNumPhysicalCpus() - 2);
}

void RenderThreadPool::start (int numWorkers, int blockSize, double sampleRate, juce::AudioWorkgroup workgroup_)
{
    stop();

    numWorkers = juce::jlimit (0, maxWorkers, numWorkers);
    workgroup = std::move (workgroup_);

    loads.reset (blockSize, sampleRate);
    stallTicks = 2 * loads.getBlockTicks();

    // the device's own IO thread asks the same of the scheduler: a period
    // of one block, with up to the whole block as computation time
    const auto blockSeconds = sampleRate > 0.0 ? blockSize / sampleRate : 0.0;
    const auto options = juce::Thread::RealtimeOptions()
                             .withPriority (9)
                             .withApproximateAudioProcessingTime (blockSize, sampleRate)
                             .withPeriodMs (blockSeconds * 1000.0);

    for (auto i = 1; i <= numWorkers; ++i)
    {
        auto worker = std::make_unique<Worker> (*this, i);

        // no real-time scheduling for this user (Linux without an rtprio
        // limit): the worker still runs, at the highest ordinary priority
        if (! worker->startRealtimeThread (options))
            worker->startThread (juce::Thread::Priority::highest);

        workers.push_back (std::move (worker));
    }

    running.store (true, std::memory_order_release);
}

void RenderThreadPool::stop()
{
    running.store (false, std::memory_order_release);

    for (auto& worker : workers)
    {
        worker->signalThreadShouldExit();
        worker->signal();
    }

    for (auto& worker : workers)
        worker->waitForThreadToExit (-1);

    workers.clear();
    workgroup.reset();
}

void RenderThreadPool::run (int numJobs, JobFunction fn, void* context) noexcept
{
    if (numJobs <= 0)
        return;

    // one path for every case: with no workers, or one job, nobody is woken,
    // this thread takes every job and the drain finds nothing to wait for
    loads.beginStage();
    publishStage (numJobs, fn, context);

    // one job stays with this thread; wake no more workers than there are jobs for
    wakeWorkers (std::min (static_cast<int> (workers.size()), numJobs - 1));
    takeJobs (0);

    const auto spinTicks = waitForStageToDrain();
    loads.endStage (spinTicks);
}

void RenderThreadPool::publishStage (int numJobs, JobFunction fn, void* context) noexcept
{
    ++generation;
    auto& stage = stages[generation & 1];
    stage.fn = fn;
    stage.context = context;
    stage.numJobs = numJobs;

    jobsRemaining.store (numJobs, std::memory_order_relaxed);
    nextTicket.store (Ticket::encode (generation, 0), std::memory_order_release);
}

void RenderThreadPool::wakeWorkers (int count) noexcept
{
    for (auto i = 0; i < count; ++i)
        workers[static_cast<size_t> (i)]->signal();
}

void RenderThreadPool::takeJobs (int workerId) noexcept
{
    participants.fetch_add (1, std::memory_order_acq_rel);
    juce::int64 busy = 0;

    for (;;)
    {
        const auto ticket = Ticket::decode (nextTicket.fetch_add (1, std::memory_order_acq_rel));
        const auto& stage = stages[ticket.generation & 1];

        if (ticket.job >= static_cast<std::uint32_t> (stage.numJobs))
            break;

        const auto start = juce::Time::getHighResolutionTicks();
        // will call eg. AudioBusRenderer<SampleType>::renderTrack
        stage.fn (stage.context, static_cast<int> (ticket.job), workerId);
        busy += juce::Time::getHighResolutionTicks() - start;
        jobsRemaining.fetch_sub (1, std::memory_order_acq_rel);
    }

    // a late worker that drew a ticket of a finished stage adds nothing:
    // its busy is zero, and the stage it would credit is already recorded
    loads.addBusy (workerId, busy);
    participants.fetch_sub (1, std::memory_order_acq_rel);
}

juce::int64 RenderThreadPool::waitForStageToDrain() noexcept
{
    const auto drained = [this]
    {
        return jobsRemaining.load (std::memory_order_acquire) == 0
            && participants.load (std::memory_order_acquire) == 0;
    };

    // nothing in flight (no workers, or none woken): no wait to measure
    if (drained())
        return 0;

    // the remaining jobs are in flight on workers: wait for them, and for
    // every worker to have left the stage, before the next run() may reuse
    // this generation's entry; the acquire on participants also completes
    // the workers' busy counts for the load meter
    const auto waitStart = juce::Time::getHighResolutionTicks();
    auto stalled = false;

    while (! drained())
    {
        if (stalled)
        {
            // a worker is held up (a lock, a starved core): spinning on at
            // real-time priority could keep it from the core it needs, so
            // from here on this thread gives its core away each turn
            std::this_thread::yield();
            continue;
        }

        cpuRelax();

        if (juce::Time::getHighResolutionTicks() - waitStart > stallTicks)
        {
            stalled = true;
            stalls.fetch_add (1, std::memory_order_relaxed);
        }
    }

    return juce::Time::getHighResolutionTicks() - waitStart;
}

} // namespace audium
