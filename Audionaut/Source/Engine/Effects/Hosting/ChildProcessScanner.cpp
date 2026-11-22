//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "ChildProcessScanner.h"

namespace audium
{

namespace
{
    /// Kills the child when the deadline passes or the scan is abandoned, while the caller reads its output.
    class Watchdog final : public juce::Thread
    {
    public:
        Watchdog (juce::ChildProcess& child_, const juce::KnownPluginList::CustomScanner& scanner_, int timeoutMs) :
            juce::Thread ("plugin scan watchdog"),
            child (child_),
            scanner (scanner_),
            deadline (juce::Time::getMillisecondCounter() + static_cast<juce::uint32> (timeoutMs))
        {
        }

        bool timedOut() const noexcept { return killed.load(); }

        void run() override
        {
            while (! threadShouldExit())
            {
                if (juce::Time::getMillisecondCounter() >= deadline || scanner.shouldExit())
                {
                    killed.store (true);
                    child.kill();
                    return;
                }
                wait (50);
            }
        }

    private:
        juce::ChildProcess& child;
        const juce::KnownPluginList::CustomScanner& scanner;
        juce::uint32 deadline;
        std::atomic<bool> killed { false };
    };
}

//==============================================================================
// The pipe carries frames: four bytes of length, then that much JSON.
namespace
{
    constexpr auto pollMs = 50;
    constexpr auto frameIoTimeoutMs = 5000;
    constexpr juce::uint32 largestFrame = 64 * 1024 * 1024;

    bool writeFrame (juce::NamedPipe& pipe, const nlohmann::json& message)
    {
        const auto text = message.dump();
        juce::MemoryOutputStream frame;
        frame.writeInt (static_cast<int> (text.size()));   // little-endian
        frame.write (text.data(), text.size());

        // in one piece: the reader takes the length whole or not at all
        return pipe.write (frame.getData(), static_cast<int> (frame.getDataSize()), frameIoTimeoutMs)
            == static_cast<int> (frame.getDataSize());
    }

    enum class Read { frame, nothing, broken };

    /// All of it, in however many pieces the pipe hands it over: a Windows pipe returns what is there,
    /// which may be less than was asked for.
    bool readAll (juce::NamedPipe& pipe, char* destination, int size, int withinMs)
    {
        const auto deadline = juce::Time::currentTimeMillis() + withinMs;
        auto done = 0;
        while (done < size)
        {
            const auto left = static_cast<int> (deadline - juce::Time::currentTimeMillis());
            if (left <= 0)
                return false;

            const auto read = pipe.read (destination + done, size - done, left);
            if (read < 0)
                return false;
            if (read == 0)
                juce::Thread::sleep (1);
            done += read;
        }
        return true;
    }

    Read readFrame (juce::NamedPipe& pipe, nlohmann::json& message, int waitMs)
    {
        // The wait is for one byte: a read that runs out of time gives up what it had taken, and one
        // byte is there or it is not. The rest of a frame that began follows at once (it is written
        // in one piece).
        char header[4];
        if (pipe.read (header, 1, waitMs) != 1)
            return Read::nothing;   // nothing yet (or a pipe that is gone: its owner sees that by other means)
        if (! readAll (pipe, header + 1, 3, frameIoTimeoutMs))
            return Read::broken;

        const auto size = juce::ByteOrder::littleEndianInt (header);
        if (size == 0 || size > largestFrame)
            return Read::broken;

        std::string text (size, '\0');
        if (! readAll (pipe, text.data(), static_cast<int> (size), frameIoTimeoutMs))
            return Read::broken;

        message = nlohmann::json::parse (text, nullptr, false);
        return message.is_object() ? Read::frame : Read::broken;
    }

    juce::String newPipeName()
    {
        const auto name = "audionaut-scan-" + juce::Uuid().toString();
       #if JUCE_WINDOWS
        return name;   // the system's pipe namespace
       #else
        // a fifo: where this process may write (a sandboxed app's /tmp is not that place)
        return juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (name).getFullPathName();
       #endif
    }

    void addDescriptions (const nlohmann::json& plugins, juce::OwnedArray<juce::PluginDescription>& result)
    {
        for (const auto& element : plugins)
        {
            if (! element.is_string())
                continue;
            if (auto xml = juce::parseXML (juce::String (element.get<std::string>())))
            {
                auto description = std::make_unique<juce::PluginDescription>();
                if (description->loadFromXml (*xml))
                    result.add (description.release());
            }
        }
    }

    /// The envelope a one-shot worker wrote: the last thing on stdout, running to its end. A plugin may
    /// have printed before it, braces included (the worker keeps its own output off stdout, not a
    /// plugin's printf): the envelope starts at the first brace from which the rest reads as one.
    bool readEnvelope (const juce::String& output, nlohmann::json& envelope)
    {
        const auto text = output.toStdString();
        for (auto start = text.find ('{'); start != std::string::npos; start = text.find ('{', start + 1))
        {
            envelope = nlohmann::json::parse (text.begin() + static_cast<std::string::difference_type> (start), text.end(),
                                              nullptr, false);
            if (envelope.is_object() && envelope.contains ("ok"))
                return true;
        }
        return false;
    }

    std::string errorCodeOf (const nlohmann::json& envelope)
    {
        return envelope.contains ("error") && envelope["error"].is_object() ? envelope["error"].value ("code", std::string()) : std::string();
    }

    /// The worker ran and has no such format: nothing in the file for it, and no crash.
    bool declinedTheFormat (const nlohmann::json& envelope)
    {
        return errorCodeOf (envelope) == "unknown_format";
    }

    /// For the log, what a worker that ended with an error had to say about it: its own message where
    /// it still wrote its envelope (a plugin it could not load, a format it has not), a crash where not.
    juce::String whatTheWorkerSaid (const juce::String& output)
    {
        nlohmann::json envelope;
        if (! readEnvelope (output, envelope))
            return " and no answer (a crash)";
        if (envelope.contains ("error") && envelope["error"].is_object())
            if (const auto message = envelope["error"].value ("message", std::string()); ! message.empty())
                return ": " + juce::String (message);
        return {};
    }

    /// A plugin loaded in this process, and what it said of itself. Shared between the scanning thread
    /// and the message that does the loading: whichever is done with it last lets it go.
    struct LoadedHere
    {
        juce::WaitableEvent done;
        std::atomic<bool> abandoned { false };   // the scan no longer waits: nothing is loaded for it any more
        std::unique_ptr<juce::PluginDescription> description;
        juce::String error;
    };

    /// Message thread (or a process without one): the plugin is created, asked, and let go again here.
    void loadOnThisThread (juce::AudioPluginFormat& format, const juce::String& fileOrIdentifier, LoadedHere& loaded)
    {
        juce::PluginDescription wanted;
        wanted.pluginFormatName = format.getName();
        wanted.fileOrIdentifier = fileOrIdentifier;
        if (auto instance = format.createInstanceFromDescription (wanted, 44100.0, 512, loaded.error))
            loaded.description = std::make_unique<juce::PluginDescription> (instance->getPluginDescription());
    }

    /// This executable as the serving worker.
    class ProcessWorker final : public ChildProcessScanner::Worker
    {
    public:
        explicit ProcessWorker (juce::File executable_) : executable (std::move (executable_)) {}

        bool start (const juce::String& pipeName) override
        {
            return child.start (juce::StringArray { executable.getFullPathName(), "scan-plugin", "--serve", pipeName }, 0);
        }

        bool isRunning() override { return child.isRunning(); }

        void kill() override
        {
            child.kill();
            child.waitForProcessToFinish (500);   // collected, so that nothing of it stays behind
        }

    private:
        juce::File executable;
        juce::ChildProcess child;
    };
}

//==============================================================================
ChildProcessScanner::ChildProcessScanner (juce::File executable_, int timeoutMs_) :
    executable (std::move (executable_)),
    workerFactory ([file = executable] { return std::make_unique<ProcessWorker> (file); }),
    timeoutMs (timeoutMs_),
    helloTimeoutMs (15000)
{
}

ChildProcessScanner::ChildProcessScanner (WorkerFactory workerFactory_, int timeoutMs_, int helloTimeoutMs_) :
    workerFactory (std::move (workerFactory_)),
    timeoutMs (timeoutMs_),
    helloTimeoutMs (helloTimeoutMs_)
{
}

ChildProcessScanner::~ChildProcessScanner()
{
    const juce::ScopedLock lock (workerLock);
    if (worker == nullptr)
        return;

    // told to leave, and given a moment to do so by itself
    if (pipe != nullptr && worker->isRunning() && writeFrame (*pipe, { { "quit", true } }))
    {
        const auto patience = juce::Time::getMillisecondCounter() + 1000;
        while (worker->isRunning() && juce::Time::getMillisecondCounter() < patience)
            juce::Thread::sleep (10);
    }
    dropWorker (true);
}

bool ChildProcessScanner::findPluginTypesFor (juce::AudioPluginFormat& format,
                                              juce::OwnedArray<juce::PluginDescription>& result,
                                              const juce::String& fileOrIdentifier)
{
    const juce::ScopedLock lock (workerLock);

    const auto before = result.size();
    const auto started = juce::Time::getMillisecondCounter();
    lastFailure.clear();

    // one line per file: what it is, what came of it, how long it took
    const auto logged = [&] (bool verdict, juce::String outcome)
    {
        if (log != nullptr)
        {
            juce::StringArray names;
            for (auto i = before; i < result.size(); ++i)
                names.add (result[i]->name);
            if (! names.isEmpty())
                outcome << ": " << names.joinIntoString (", ");

            log (format.getName() + "  " + fileOrIdentifier + "  ->  " + outcome
                 + "  (" + juce::String (juce::Time::getMillisecondCounter() - started) + " ms)");
        }
        return verdict;
    };
    const auto found = [&]
    {
        if (result.size() != before)
            return juce::String (result.size() - before) + " found";

        // loaded, and nothing in it
        if (foundNothing != nullptr)
            foundNothing (fileOrIdentifier);
        return juce::String ("no plugin found");
    };
    const auto failure = [&] { return "FAILED, file skipped: " + (lastFailure.isNotEmpty() ? lastFailure : juce::String ("the worker gave no answer")); };

    // A plugin the system asks the user about is no worker's to load: the system opens it in the app
    // itself only, and it is the app's user who is asked. So it is loaded here, in this process, where
    // that is allowed - and left as it is listed where it is not (a command run without the app).
    if (asksUser && asksUser (format.getName(), fileOrIdentifier))
    {
        if (loweredSecurity == nullptr || ! loweredSecurity->load())
            return logged (true, "NOT SCANNED: macOS asks the user before it loads this plugin, which only the app may have it do");
        if (declinedInThisScan)
            return logged (true, "NOT SCANNED: lowering the security settings was declined earlier in this scan");

        // The plugin lives on the message thread from its creation to its end, as every plugin of the
        // app does: a scan runs on a thread of its own, and a plugin made on one thread and asked and
        // destroyed on another, inside the app, has no worker to take the fall. The scanning thread
        // only waits - and gives up when the scan is abandoned: the message thread may be the one
        // waiting for this thread to end (a scan window that closes), and would never get to the load.
        auto loaded = std::make_shared<LoadedHere>();
        auto* messageManager = juce::MessageManager::getInstanceWithoutCreating();
        if (messageManager == nullptr || messageManager->isThisTheMessageThread())
        {
            loadOnThisThread (format, fileOrIdentifier, *loaded);
        }
        else
        {
            // the format is the host's, as this scanner is (through its list), and both go on the
            // message thread: while the scanner lives there, so does the format
            juce::MessageManager::callAsync ([loaded, &format, fileOrIdentifier, scannerAlive = std::weak_ptr<int> (alive)]
            {
                if (! loaded->abandoned.load() && scannerAlive.lock() != nullptr)
                    loadOnThisThread (format, fileOrIdentifier, *loaded);
                loaded->done.signal();
            });

            while (! loaded->done.wait (pollMs))
            {
                if (shouldExit())
                {
                    loaded->abandoned.store (true);
                    return logged (true, "scan cancelled");   // no verdict on the file
                }
            }
        }

        const auto& error = loaded->error;
        if (loaded->description != nullptr)
        {
            result.add (loaded->description.release());
            return logged (true, found() + ", in this process");
        }

        // The system's own word that the user said no (userCanceledErr, in the reason the format gives):
        // that holds for the rest of this scan, so the question is not raised for every further plugin.
        // The file is not skipped from now on - the next scan asks again.
        if (error.contains ("(-128)"))
        {
            declinedInThisScan = true;
            return logged (true, "NOT SCANNED: lowering the security settings was declined; plugins that need those settings are left out for the rest of this scan");
        }

        // not loaded for another reason: said, and the plugin stays listed as it was - what keeps the
        // app from opening it today (a build without the entitlement, say) need not do so tomorrow
        return logged (true, "NOT SCANNED: not loaded in this process" + (error.isNotEmpty() ? ": " + error : juce::String()));
    }

    if (! pipeUnusable)
    {
        auto asked = askWorker (format.getName(), fileOrIdentifier, result);

        // A worker that had other plugins in it may have died of what they left behind, not of this
        // file: the file gets a worker to itself before the failure is held against it.
        if (asked == Asked::failedAfterOthers)
        {
            note ("  " + fileOrIdentifier + ": " + lastFailure + "; asked again of a new worker, the first had loaded other plugins");
            asked = askWorker (format.getName(), fileOrIdentifier, result);
        }

        switch (asked)
        {
            case Asked::found:             return logged (true, found());
            case Asked::nothing:           return logged (true, "not a format the worker hosts");
            case Asked::failed:
            case Asked::failedAfterOthers: return logged (false, failure());   // the plugin took the worker down, or kept it too long
            case Asked::abandoned:         return logged (true, "scan cancelled");   // by the user: no verdict on the file
            case Asked::needsApp:          return logged (true, "NOT SCANNED: " + lastFailure);   // nothing tried it: no verdict either
            case Asked::unavailable:       break;          // no worker this time: this file in a process of its own
            case Asked::unusable:
                pipeUnusable = true;
                note ("the worker's pipe does not work here: from now on one worker process per file");
                break;
        }
    }

    lastFailure.clear();
    noVerdict = false;
    const auto verdict = scanInProcessOfItsOwn (format, result, fileOrIdentifier);
    if (noVerdict)   // the worker is what failed, not the plugin: the file is not skipped from now on
        return logged (true, "NOT SCANNED: " + lastFailure);
    return logged (verdict, (verdict ? found() : failure()) + ", in a worker process of its own");
}

void ChildProcessScanner::scanFinished()
{
    const juce::ScopedLock lock (workerLock);
    declinedInThisScan = false;
}

ChildProcessScanner::Asked ChildProcessScanner::askWorker (const juce::String& formatName, const juce::String& fileOrIdentifier,
                                                           juce::OwnedArray<juce::PluginDescription>& result)
{
    // the worker gives up a little before the scanner does, so that its answer still gets here
    const nlohmann::json request { { "format", formatName.toStdString() },
                                   { "file", fileOrIdentifier.toStdString() },
                                   { "timeoutMs", workerTimeoutMs (timeoutMs) } };

    // the worker is not left to itself after a failure: a plugin may be stuck in it
    const auto failed = [this] (const juce::String& why)
    {
        lastFailure = why;
        const auto afterOthers = answeredByWorker > 0;
        dropWorker (true);
        return afterOthers ? Asked::failedAfterOthers : Asked::failed;
    };

    // A worker that was idle may be on its way out just as the request goes to it: its goodbye comes
    // back in place of an answer, or the request cannot be written. The next worker gets the request.
    for (auto attempt = 0; attempt < 3; ++attempt)
    {
        if (worker == nullptr || ! worker->isRunning())
        {
            switch (startWorker())
            {
                case Started::ready:     startFailures = 0; break;
                case Started::abandoned: return Asked::abandoned;
                case Started::silent:    return Asked::unusable;   // the pipe does not work here
                case Started::failed:    return ++startFailures >= 2 ? Asked::unusable : Asked::unavailable;
            }
        }

        if (! writeFrame (*pipe, request))
        {
            dropWorker (true);
            continue;
        }

        const auto deadline = juce::Time::getMillisecondCounter() + static_cast<juce::uint32> (timeoutMs);
        auto gone = false;
        for (;;)
        {
            nlohmann::json answer;
            const auto read = readFrame (*pipe, answer, pollMs);

            if (read == Read::frame)
            {
                if (answer.contains ("bye"))
                    break;   // it left without taking the request

                // a plugin the worker leaves to the app: it loaded nothing, and goes on
                if (! answer.value ("ok", false) && answer.value ("code", std::string()) == "needs_app")
                {
                    lastFailure = juce::String (answer.value ("message", std::string ("only the app itself loads this plugin")));
                    return Asked::needsApp;
                }

                if (! answer.value ("ok", false))
                    return answer.value ("code", std::string()) == "unknown_format"
                               ? Asked::nothing
                               : failed (juce::String (answer.value ("message", answer.value ("code", std::string ("the worker reported an error")))));

                ++answeredByWorker;
                if (answer.contains ("plugins") && answer["plugins"].is_array())
                    addDescriptions (answer["plugins"], result);
                return Asked::found;
            }

            if (read == Read::broken || gone)
                return failed (gone ? "the worker process ended while the plugin was loading (a crash)" : "the worker's answer was unreadable");

            if (shouldExit())
            {
                dropWorker (true);
                return Asked::abandoned;
            }

            if (juce::Time::getMillisecondCounter() >= deadline)
                return failed ("no answer within " + juce::String (timeoutMs / 1000) + " s");

            // a worker that died may have answered first: the pipe is looked at once more
            gone = ! worker->isRunning();
        }

        dropWorker (true);
    }

    return Asked::unavailable;
}

int ChildProcessScanner::workerTimeoutMs (int scannerTimeoutMs) noexcept
{
    return scannerTimeoutMs - scannerTimeoutMs / 6;
}

ChildProcessScanner::Started ChildProcessScanner::startWorker()
{
    dropWorker (true);

    pipe = std::make_unique<juce::NamedPipe>();
    const auto name = newPipeName();
    if (! pipe->createNewPipe (name, true))
    {
        pipe.reset();
        note ("worker not started: the pipe could not be created (" + name + ")");
        return Started::failed;
    }

    worker = workerFactory();
    if (worker == nullptr || ! worker->start (name))
    {
        dropWorker (false);
        note ("worker not started: the process could not be launched");
        return Started::failed;
    }
    ++workersStarted;
    const auto launched = juce::Time::getMillisecondCounter();

    // its hello says that the pipe works in both directions
    const auto deadline = juce::Time::getMillisecondCounter() + static_cast<juce::uint32> (helloTimeoutMs);
    auto gone = false;
    for (;;)
    {
        nlohmann::json hello;
        if (readFrame (*pipe, hello, pollMs) == Read::frame && hello.contains ("hello"))
        {
            workerEverAnswered = true;
            note ("worker " + juce::String (workersStarted.load()) + " started (" + juce::String (juce::Time::getMillisecondCounter() - launched) + " ms)");
            return Started::ready;
        }

        // only a worker that is there and stays silent says something about the pipe
        const auto outcome = gone ? Started::failed
                           : shouldExit() ? Started::abandoned
                           : juce::Time::getMillisecondCounter() >= deadline ? Started::silent
                           : Started::ready;
        if (outcome != Started::ready)
        {
            note (outcome == Started::failed ? "worker " + juce::String (workersStarted.load()) + " ended before it was ready"
                : outcome == Started::silent ? "worker " + juce::String (workersStarted.load()) + " did not report ready within " + juce::String (helloTimeoutMs / 1000) + " s"
                                             : juce::String ("scan cancelled while a worker was starting"));
            dropWorker (true);
            return outcome;
        }
        gone = ! worker->isRunning();
    }
}

void ChildProcessScanner::note (const juce::String& line) const
{
    if (log != nullptr)
        log (line);
}

void ChildProcessScanner::dropWorker (bool kill)
{
    if (worker != nullptr && kill)
        worker->kill();
    worker.reset();
    pipe.reset();
    answeredByWorker = 0;
}

bool ChildProcessScanner::scanInProcessOfItsOwn (juce::AudioPluginFormat& format,
                                                 juce::OwnedArray<juce::PluginDescription>& result,
                                                 const juce::String& fileOrIdentifier)
{
    if (executable == juce::File())
    {
        lastFailure = "no worker available";
        noVerdict = true;
        return true;
    }

    juce::ChildProcess child;
    juce::StringArray arguments { executable.getFullPathName(), "scan-plugin", "--json",
                                  "--timeout-ms", juce::String (workerTimeoutMs (timeoutMs)) };
    arguments.addArray ({ "--format", format.getName(), fileOrIdentifier });
    if (! child.start (arguments, juce::ChildProcess::wantStdOut))
    {
        lastFailure = "the worker process could not be launched";
        noVerdict = true;
        return true;
    }

    Watchdog watchdog (child, *this, timeoutMs);
    watchdog.startThread();

    // read to the end before waiting: a long answer would fill the pipe and stall the child
    const auto output = child.readAllProcessOutput();
    child.waitForProcessToFinish (1000);
    watchdog.stopThread (2000);

    if (watchdog.timedOut())
        lastFailure = "no answer within " + juce::String (timeoutMs / 1000) + " s";
    else if (child.getExitCode() != 0)
        lastFailure = "the worker process ended with code " + juce::String (static_cast<int> (child.getExitCode())) + whatTheWorkerSaid (output);
    else
        lastFailure = "the worker's answer was unreadable";

    if (watchdog.timedOut())
        return shouldExit();   // abandoned by the user: no verdict on the file; out of time: a crash as far as the list is concerned

    nlohmann::json envelope;
    const auto answered = readEnvelope (output, envelope);

    // A worker that ends without a word, where none has ever got as far as an answer, is a worker that
    // does not run here (missing, not signed as it must be, stopped by the system) - not a plugin that
    // took it down. Held against the files, that would skip every plugin of the machine.
    if (! answered && ! workerEverAnswered)
    {
        lastFailure << "; no worker has run here yet, so this is not held against the plugin";
        noVerdict = true;
        return true;
    }
    workerEverAnswered = workerEverAnswered || answered;

    // the worker left the plugin to the app and loaded nothing: not a crash, and not an empty file
    if (answered && errorCodeOf (envelope) == "needs_app")
    {
        lastFailure = "only the app itself loads this plugin";
        if (envelope.contains ("error") && envelope["error"].is_object())
            lastFailure = juce::String (envelope["error"].value ("message", lastFailure.toStdString()));
        noVerdict = true;
        return true;
    }

    if (child.getExitCode() != 0)
        return answered && declinedTheFormat (envelope);   // the plugin took the worker down - unless the worker said that it has no such format

    return parseScanEnvelope (output, result);
}

//==============================================================================
int ChildProcessScanner::serve (const juce::String& pipeName, const ScanFunction& scan, int idleMs,
                                const std::function<void()>& pump)
{
    juce::NamedPipe pipe;
    if (! pipe.openExisting (pipeName) || ! writeFrame (pipe, { { "hello", 1 } }))
        return 1;

    auto lastAsked = juce::Time::getMillisecondCounter();
    for (;;)
    {
        nlohmann::json request;
        const auto read = readFrame (pipe, request, pollMs);

        if (read == Read::broken)
            return 1;

        if (read == Read::nothing)
        {
            if (pump)
                pump();

            // nobody asks any more (the scan is over, or the scanner is gone): said, so that a request
            // written just now is not taken for one this worker died of
            if (juce::Time::getMillisecondCounter() - lastAsked >= static_cast<juce::uint32> (idleMs))
            {
                writeFrame (pipe, { { "bye", true } });
                return 0;
            }
            continue;
        }

        if (request.contains ("quit"))
            return 0;

        const auto answer = scan ({ juce::String (request.value ("format", std::string())),
                                    juce::String (request.value ("file", std::string())),
                                    request.value ("timeoutMs", 50000) });
        if (! writeFrame (pipe, answer))
            return 1;
        if (answer.value ("last", false))
            return answer.value ("ok", false) ? 0 : 1;

        lastAsked = juce::Time::getMillisecondCounter();
    }
}

bool ChildProcessScanner::parseScanEnvelope (const juce::String& output, juce::OwnedArray<juce::PluginDescription>& result)
{
    nlohmann::json envelope;
    if (! readEnvelope (output, envelope))
        return false;
    if (! envelope["ok"].is_boolean() || ! envelope["ok"].get<bool>())
        return declinedTheFormat (envelope);   // nothing in the file for a format the worker has not; any other failure counts as a crash

    if (! envelope.contains ("result") || ! envelope["result"].is_object()
        || ! envelope["result"].contains ("plugins") || ! envelope["result"]["plugins"].is_array())
        return false;

    addDescriptions (envelope["result"]["plugins"], result);
    return true;
}

} // namespace audium
