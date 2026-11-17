//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include <nlohmann/json.hpp>

namespace audium
{

/**
 * Scans plugin files in a process of its own: an executable run as the
 * `scan-plugin` worker (ScanWorker). A plugin that crashes while loading takes that
 * process down, not the app, and the scan goes on; the file is then
 * blacklisted, as JUCE does for a scanner that answers false. The same when
 * the worker is not done with a file within the timeout.
 *
 * One worker serves many files (`scan-plugin --serve <pipe>`): the scanner
 * hands it one file at a time over a named pipe and reads the answer from
 * it, so starting the binary is paid once and not per file. Only one file
 * is ever on its way, which keeps the verdict exact: the worker that dies
 * died while that file was in it, and the next file gets a new worker. A
 * file that failed in a worker which had loaded other plugins before is
 * asked once more of a new worker, and only blacklisted when it fails there
 * too: what an earlier plugin left behind is not held against it. Where the
 * system may ask the user about a plugin, a file such a worker found nothing
 * in is likewise asked of a new one. A worker nobody asks for a while leaves
 * by itself.
 *
 * Where the pipe does not work (the worker is there and never says hello,
 * or does not start twice in a row), the scanner falls back to a process
 * per file: `scan-plugin --json --format <name>
 * <file or identifier>`, which reports one JSON envelope on stdout and
 * exits.
 *
 * The app binary doubles as the worker (it runs CLI verbs headlessly) -
 * except in the sandboxed macOS app, whose own binary the system does not
 * let it start as a child: there the worker is the scan helper in the
 * bundle (Audionaut/ScanHelper, PluginHost::scanWorkerExecutable).
 */
class ChildProcessScanner final : public juce::KnownPluginList::CustomScanner
{
public:
    /// The other end of the pipe: a child process, or what stands in for one in a test.
    class Worker
    {
    public:
        virtual ~Worker() = default;
        /// Starts it; it opens the pipe of that name and serves it.
        virtual bool start (const juce::String& pipeName) = 0;
        virtual bool isRunning() = 0;
        virtual void kill() = 0;
    };

    using WorkerFactory = std::function<std::unique_ptr<Worker>()>;

    explicit ChildProcessScanner (juce::File executable, int timeoutMs = 60000);
    /// With workers of the caller's making, and no process per file to fall back to.
    ChildProcessScanner (WorkerFactory workerFactory, int timeoutMs, int helloTimeoutMs = 15000);
    ~ChildProcessScanner() override;

    bool findPluginTypesFor (juce::AudioPluginFormat& format,
                             juce::OwnedArray<juce::PluginDescription>& result,
                             const juce::String& fileOrIdentifier) override;

    /// Whether the worker may load plugins that make the system ask the user to lower the app's security
    /// settings (PluginHost::setLoweredSecurityAllowed); read for every file. Without a flag: no.
    void setLoweredSecurityFlag (std::shared_ptr<const std::atomic<bool>> flag) { loweredSecurity = std::move (flag); }

    /// Where the scanner says what it does, a line at a time: every file with what came of it and how
    /// long it took, and the workers' coming and going. Called on the scanning thread.
    void setLog (std::function<void (const juce::String&)> log_) { log = std::move (log_); }

    /// How many workers were started so far.
    int getNumWorkersStarted() const noexcept { return workersStarted.load(); }

    //==============================================================================
    /// What the worker is asked: one file of one format, so long for it, and whether it may load a
    /// plugin the system wants the user's consent for.
    struct Request
    {
        juce::String format;
        juce::String file;
        int timeoutMs = 50000;
        bool loweredSecurity = false;
    };

    /// The worker's side: what it found. `{"ok": true, "plugins": [xml, ...]}`, or
    /// `{"ok": false, "code": ...}`; with `"last": true` the worker leaves after this answer.
    using ScanFunction = std::function<nlohmann::json (const Request& request)>;

    /// How long the worker gets for a file the scanner waits timeoutMs for: a little less, so that
    /// its own answer about a stuck plugin still arrives.
    static int workerTimeoutMs (int scannerTimeoutMs) noexcept;

    /// The worker's loop: opens the pipe, says hello, answers one request after the other and leaves
    /// when it is told to, when an answer was its last, or when nobody asked for idleMs. `pump` is
    /// called while it waits. Returns the process's exit code.
    static int serve (const juce::String& pipeName, const ScanFunction& scan, int idleMs = 5000,
                      const std::function<void()>& pump = {});

    /// The one-shot worker's stdout to descriptions; false when it is not the envelope the worker writes (a crash mid-write).
    static bool parseScanEnvelope (const juce::String& output, juce::OwnedArray<juce::PluginDescription>& result);

private:
    enum class Asked { found, nothing, failed, failedAfterOthers, abandoned, unavailable, unusable };
    enum class Started { ready, silent, failed, abandoned };

    Asked askWorker (const juce::String& formatName, const juce::String& fileOrIdentifier,
                     juce::OwnedArray<juce::PluginDescription>& result);
    Started startWorker();
    void dropWorker (bool kill);
    void note (const juce::String& line) const;
    bool scanInProcessOfItsOwn (juce::AudioPluginFormat& format, juce::OwnedArray<juce::PluginDescription>& result,
                                const juce::String& fileOrIdentifier);

    juce::File executable;
    WorkerFactory workerFactory;
    int timeoutMs;
    int helloTimeoutMs;
    std::shared_ptr<const std::atomic<bool>> loweredSecurity;

    juce::CriticalSection workerLock;          // one file on its way at a time
    std::unique_ptr<juce::NamedPipe> pipe;
    std::unique_ptr<Worker> worker;
    int answeredByWorker = 0;                  // files the current worker has loaded
    int startFailures = 0;                     // workers in a row that did not get as far as hello
    int emptyAgainInNewWorker = 0;             // files a new worker found as empty as the one before it, since the last file with a plugin
    bool pipeUnusable = false;                 // the fallback, for the rest of this scanner's life
    std::atomic<int> workersStarted { 0 };
    std::function<void (const juce::String&)> log;
    juce::String lastFailure;                  // why the file on its way failed, for the log

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChildProcessScanner)
};

} // namespace audium
