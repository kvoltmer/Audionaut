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
 * Scans plugin files in a process of its own: this executable, run as the
 * `scan-plugin` worker. A plugin that crashes while loading takes that
 * process down, not the app, and the scan goes on; the file is then
 * blacklisted, as JUCE does for a scanner that answers false. The same when
 * the worker is not done with a file within the timeout.
 *
 * One worker serves many files (`scan-plugin --serve <pipe>`): the scanner
 * hands it one file at a time over a named pipe and reads the answer from
 * it, so starting the binary is paid once and not per file. Only one file
 * is ever on its way, which keeps the verdict exact: the worker that dies
 * died of that file, and the next file gets a new worker. A worker nobody
 * asks for a while leaves by itself.
 *
 * Where the pipe does not work (the worker never says hello), the scanner
 * falls back to a process per file: `scan-plugin --json --format <name>
 * <file or identifier>`, which reports one JSON envelope on stdout and
 * exits.
 *
 * The app binary doubles as the worker (it runs CLI verbs headlessly), so
 * no helper executable goes into the bundle.
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

    /// How many workers were started so far.
    int getNumWorkersStarted() const noexcept { return workersStarted.load(); }

    //==============================================================================
    /// The worker's side: what it found in one file of one format. `{"ok": true, "plugins": [xml, ...]}`,
    /// or `{"ok": false, "code": ...}`; with `"last": true` the worker leaves after this answer.
    using ScanFunction = std::function<nlohmann::json (const juce::String& formatName, const juce::String& fileOrIdentifier)>;

    /// The worker's loop: opens the pipe, says hello, answers one request after the other and leaves
    /// when it is told to, when an answer was its last, or when nobody asked for idleMs. `pump` is
    /// called while it waits. Returns the process's exit code.
    static int serve (const juce::String& pipeName, const ScanFunction& scan, int idleMs = 5000,
                      const std::function<void()>& pump = {});

    /// The one-shot worker's stdout to descriptions; false when it is not the envelope the worker writes (a crash mid-write).
    static bool parseScanEnvelope (const juce::String& output, juce::OwnedArray<juce::PluginDescription>& result);

private:
    enum class Asked { found, nothing, failed, abandoned, unavailable };

    Asked askWorker (const juce::String& formatName, const juce::String& fileOrIdentifier,
                     juce::OwnedArray<juce::PluginDescription>& result);
    bool startWorker();
    void dropWorker (bool kill);
    bool scanInProcessOfItsOwn (juce::AudioPluginFormat& format, juce::OwnedArray<juce::PluginDescription>& result,
                                const juce::String& fileOrIdentifier);

    juce::File executable;
    WorkerFactory workerFactory;
    int timeoutMs;
    int helloTimeoutMs;

    juce::CriticalSection workerLock;          // one file on its way at a time
    std::unique_ptr<juce::NamedPipe> pipe;
    std::unique_ptr<Worker> worker;
    bool pipeUnusable = false;                 // the fallback, for the rest of this scanner's life
    std::atomic<int> workersStarted { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChildProcessScanner)
};

} // namespace audium
