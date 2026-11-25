//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Engine/Effects/Hosting/PluginListScan.h"

#include <atomic>

#include "Engine/Effects/Hosting/PluginHost.h"

namespace audium
{

/// One format's files, on a thread of its own.
struct PluginListScan::FormatScan
{
    FormatScan (PluginHost& scanned, juce::AudioPluginFormat& format) :
        scanner (scanned.getList(), format, scanned.getScanFolders (format), true, scanned.getDeadMansPedalFile(), true)
    {
        pool.addJob (new Job (*this), true);
    }

    ~FormatScan()
    {
        cancel();
        pool.removeAllJobs (true, 60000);   // the list's scanner hears of it and gives up the file at hand
    }

    /// The job is told to stop, which the list's scanner hears: it gives up the file at hand.
    void cancel()
    {
        cancelled = true;
        pool.removeAllJobs (true, 0);
        if (pool.getNumJobs() == 0)   // the job had not begun: nobody else says that it is over
            done = true;
    }

    juce::String pluginAtHand()
    {
        const juce::ScopedLock lock (nameLock);
        return name;
    }

    /// The files that gave no plugin; not the one a cancelled scan gave up, which did not fail.
    juce::StringArray failedFiles() const
    {
        auto failed = scanner.getFailedFiles();
        if (cancelled)
            failed.removeRange (failedBeforeThisFile, failed.size());
        return failed;
    }

    /// A thread pool's job, which is what the list's scanner asks whether to go on.
    struct Job final : public juce::ThreadPoolJob
    {
        explicit Job (FormatScan& scan_) : juce::ThreadPoolJob ("plugin scan"), scan (scan_) {}

        JobStatus runJob() override
        {
            juce::String ignored;
            while (! scan.cancelled && ! shouldExit())
            {
                {
                    const juce::ScopedLock lock (scan.nameLock);
                    scan.name = scan.scanner.getNextPluginFileThatWillBeScanned();   // the plugin's name, as its format knows it
                }
                scan.failedBeforeThisFile = scan.scanner.getFailedFiles().size();
                const auto more = scan.scanner.scanNextFile (true, ignored);   // false after the last file, and where there is none
                if (! shouldExit())
                    scan.failedBeforeThisFile = scan.scanner.getFailedFiles().size();   // this one had its verdict
                if (! more)
                    break;
            }
            scan.done = true;
            return jobHasFinished;
        }

        FormatScan& scan;
    };

    juce::PluginDirectoryScanner scanner;
    juce::CriticalSection nameLock;
    juce::String name;   // of the plugin at hand, under the lock
    std::atomic<int> failedBeforeThisFile { 0 };
    std::atomic<bool> cancelled { false }, done { false };
    juce::ThreadPool pool { juce::ThreadPoolOptions{}.withNumberOfThreads (1) };   // one worker process at a time
};

//==============================================================================
PluginListScan::PluginListScan (PluginHost& host_, std::vector<juce::AudioPluginFormat*> formats_,
                                Progress onProgress_, Finished onFinished_) :
    host (host_),
    formats (std::move (formats_)),
    onProgress (std::move (onProgress_)),
    onFinished (std::move (onFinished_)),
    skippedBefore (host.getList().getBlacklistedFiles())
{
    for (auto* format : formats)
        host.restoreRemoved (format->getName());   // a scan brings back what was taken off the list
    host.syncWithInstalled();                      // what was installed since is scanned too
    scannedBefore = numScanned();
    host.getDeadMansPedalFile().getParentDirectory().createDirectory();

    if (! formats.empty())
        current = std::make_unique<FormatScan> (host, *formats[numStarted++]);
    startTimer (50);   // without a format the scan is over at the first tick
}

PluginListScan::~PluginListScan()
{
    stopTimer();
    if (! finished)
    {
        current.reset();     // ends the scan, before the list it fills is brought in line
        host.finishScan();
    }
}

void PluginListScan::cancel()
{
    outcome.cancelled = true;
    if (current != nullptr)
        current->cancel();
}

std::vector<juce::AudioPluginFormat*> PluginListScan::scannableFormats (PluginHost& host)
{
    std::vector<juce::AudioPluginFormat*> formats;
    for (auto* format : host.getFormats().getFormats())
        if (format->canScanForPlugins())
            formats.push_back (format);
    return formats;
}

int PluginListScan::numScanned() const
{
    auto count = 0;
    for (const auto& description : host.getList().getTypes())
        count += PluginHost::isScanned (description) ? 1 : 0;
    return count;
}

void PluginListScan::timerCallback()
{
    if (current != nullptr)
    {
        if (! current->done)
        {
            if (onProgress)
                onProgress ((static_cast<double> (numStarted - 1) + current->scanner.getProgress()) / static_cast<double> (formats.size()),
                            current->pluginAtHand());
            return;
        }

        outcome.failed.addArray (current->failedFiles());
        current.reset();
    }

    if (! outcome.cancelled && numStarted < formats.size())
    {
        current = std::make_unique<FormatScan> (host, *formats[numStarted++]);
        return;
    }

    stopTimer();
    finished = true;
    host.finishScan();   // the list as the scan left it, in line with what is installed

    for (const auto& skipped : host.getList().getBlacklistedFiles())
        if (! skippedBefore.contains (skipped))
            outcome.skipped.add (skipped);
    outcome.added = juce::jmax (0, numScanned() - scannedBefore);

    // the listener may delete this scan: nothing of it is touched after the call
    if (const auto listener = std::move (onFinished))
        listener (Outcome (outcome));
}

} // namespace audium
