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
 * too: what an earlier plugin left behind is not held against it. A worker
 * nobody asks for a while leaves by itself.
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
 *
 * One kind of plugin no worker loads: an Audio Unit the system asks the
 * user about before it opens it for a sandboxed app (setAsksUser). The
 * system opens it in the app itself only, so the scanner loads it in the
 * process it runs in, where that is allowed (setLoweredSecurityFlag) - with
 * no worker to take the fall if it crashes - and leaves it not scanned
 * where it is not. In a process with a message thread the plugin is
 * created, asked and destroyed on that thread, whichever thread scans; the
 * format handed to findPluginTypesFor must then live as long as the scanner
 * and end on the message thread (a host's formats do). A worker that is
 * asked about such a plugin all the same answers `needs_app` without
 * loading it, which leaves the file not scanned and not skipped. When the user declines the question, the further such
 * plugins of that scan are left alone, so that it comes up once.
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
    /// The list's word that a scan is over: the next one may raise the system's question again where
    /// the user declined it.
    void scanFinished() override;

    /// Which plugins the system asks the user about before it loads them here (format name, file or
    /// identifier): those are loaded in this process, not by a worker. None unless told.
    void setAsksUser (std::function<bool (const juce::String&, const juce::String&)> asks) { asksUser = std::move (asks); }
    /// Whether such plugins may be loaded at all, and the system's question with them
    /// (PluginHost::setLoweredSecurityAllowed); read for every file. Without a flag: no.
    void setLoweredSecurityFlag (std::shared_ptr<const std::atomic<bool>> flag) { loweredSecurity = std::move (flag); }

    /// Where the scanner says what it does, a line at a time: every file with what came of it and how
    /// long it took, and the workers' coming and going. Called on the scanning thread.
    void setLog (std::function<void (const juce::String&)> log_) { log = std::move (log_); }

    /// Hears of every file that was loaded and had no plugin in it - not of a file that was not scanned
    /// (no worker, a cancelled scan, a plugin the system asks the user about where that is not allowed
    /// or was declined). Called on the scanning thread.
    void setFoundNothing (std::function<void (const juce::String&)> heard) { foundNothing = std::move (heard); }

    /// How many workers were started so far.
    int getNumWorkersStarted() const noexcept { return workersStarted.load(); }

    //==============================================================================
    /// What the worker is asked: one file of one format, and so long for it.
    struct Request
    {
        juce::String format;
        juce::String file;
        int timeoutMs = 50000;
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
    enum class Asked { found, nothing, failed, failedAfterOthers, abandoned, needsApp, unavailable, unusable };
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
    std::function<bool (const juce::String&, const juce::String&)> asksUser;
    std::shared_ptr<const std::atomic<bool>> loweredSecurity;
    std::shared_ptr<int> alive = std::make_shared<int> (0);   // expires with the scanner, for a load still in the message queue

    juce::CriticalSection workerLock;          // one file on its way at a time
    std::unique_ptr<juce::NamedPipe> pipe;
    std::unique_ptr<Worker> worker;
    int answeredByWorker = 0;                  // files the current worker has loaded
    int startFailures = 0;                     // workers in a row that did not get as far as hello
    bool workerEverAnswered = false;           // a worker said hello, or a one-shot one wrote its envelope: workers do run here
    bool noVerdict = false;                    // the file on its way was not scanned for want of a worker
    bool declinedInThisScan = false;           // the user declined the system's question in this scan: it is not raised again before the next
    bool pipeUnusable = false;                 // the fallback, for the rest of this scanner's life
    std::atomic<int> workersStarted { 0 };
    std::function<void (const juce::String&)> log;
    std::function<void (const juce::String&)> foundNothing;
    juce::String lastFailure;                  // why the file on its way failed, for the log

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChildProcessScanner)
};

} // namespace audium
