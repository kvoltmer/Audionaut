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

    Read readFrame (juce::NamedPipe& pipe, nlohmann::json& message, int waitMs)
    {
        char header[4];
        if (pipe.read (header, 4, waitMs) != 4)
            return Read::nothing;   // nothing yet (or a pipe that is gone: its owner sees that by other means)

        const auto size = juce::ByteOrder::littleEndianInt (header);
        if (size == 0 || size > largestFrame)
            return Read::broken;

        std::string text (size, '\0');
        if (pipe.read (text.data(), static_cast<int> (size), frameIoTimeoutMs) != static_cast<int> (size))
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
        void kill() override { child.kill(); }

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

    if (! pipeUnusable)
    {
        switch (askWorker (format.getName(), fileOrIdentifier, result))
        {
            case Asked::found:
            case Asked::nothing:     return true;
            case Asked::failed:      return false;   // the plugin took the worker down, or kept it too long
            case Asked::abandoned:   return true;    // by the user: no verdict on the file
            case Asked::unavailable: pipeUnusable = true; break;
        }
    }

    return scanInProcessOfItsOwn (format, result, fileOrIdentifier);
}

ChildProcessScanner::Asked ChildProcessScanner::askWorker (const juce::String& formatName, const juce::String& fileOrIdentifier,
                                                           juce::OwnedArray<juce::PluginDescription>& result)
{
    const nlohmann::json request { { "format", formatName.toStdString() }, { "file", fileOrIdentifier.toStdString() } };

    // A worker that was idle may be on its way out just as the request goes to it: its goodbye comes
    // back in place of an answer, or the request cannot be written. The next worker gets the request.
    for (auto attempt = 0; attempt < 3; ++attempt)
    {
        if ((worker == nullptr || ! worker->isRunning()) && ! startWorker())
            return Asked::unavailable;

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

                if (answer.value ("last", false))
                    dropWorker (false);   // it is leaving after this answer, by its own choice

                if (! answer.value ("ok", false))
                    return answer.value ("code", std::string()) == "unknown_format" ? Asked::nothing : Asked::failed;

                if (answer.contains ("plugins") && answer["plugins"].is_array())
                    addDescriptions (answer["plugins"], result);
                return Asked::found;
            }

            if (read == Read::broken || gone)
            {
                dropWorker (true);
                return Asked::failed;
            }

            if (shouldExit())
            {
                dropWorker (true);
                return Asked::abandoned;
            }

            if (juce::Time::getMillisecondCounter() >= deadline)
            {
                dropWorker (true);
                return Asked::failed;
            }

            // a worker that died may have answered first: the pipe is looked at once more
            gone = ! worker->isRunning();
        }

        dropWorker (true);
    }

    return Asked::unavailable;
}

bool ChildProcessScanner::startWorker()
{
    dropWorker (true);

    pipe = std::make_unique<juce::NamedPipe>();
    const auto name = newPipeName();
    if (! pipe->createNewPipe (name, true))
    {
        pipe.reset();
        return false;
    }

    worker = workerFactory();
    if (worker == nullptr || ! worker->start (name))
    {
        dropWorker (false);
        return false;
    }
    ++workersStarted;

    // its hello says that the pipe works in both directions
    const auto deadline = juce::Time::getMillisecondCounter() + static_cast<juce::uint32> (helloTimeoutMs);
    auto gone = false;
    for (;;)
    {
        nlohmann::json hello;
        if (readFrame (*pipe, hello, pollMs) == Read::frame && hello.contains ("hello"))
            return true;

        if (gone || shouldExit() || juce::Time::getMillisecondCounter() >= deadline)
        {
            dropWorker (true);
            return false;
        }
        gone = ! worker->isRunning();
    }
}

void ChildProcessScanner::dropWorker (bool kill)
{
    if (worker != nullptr && kill)
        worker->kill();
    worker.reset();
    pipe.reset();
}

bool ChildProcessScanner::scanInProcessOfItsOwn (juce::AudioPluginFormat& format,
                                                 juce::OwnedArray<juce::PluginDescription>& result,
                                                 const juce::String& fileOrIdentifier)
{
    if (executable == juce::File())
        return false;

    juce::ChildProcess child;
    const juce::StringArray arguments { executable.getFullPathName(), "scan-plugin", "--json",
                                        "--format", format.getName(), fileOrIdentifier };
    if (! child.start (arguments, juce::ChildProcess::wantStdOut))
        return false;

    Watchdog watchdog (child, *this, timeoutMs);
    watchdog.startThread();

    // read to the end before waiting: a long answer would fill the pipe and stall the child
    const auto output = child.readAllProcessOutput();
    child.waitForProcessToFinish (1000);
    watchdog.stopThread (2000);

    if (watchdog.timedOut())
        return shouldExit();   // abandoned by the user: no verdict on the file; out of time: a crash as far as the list is concerned

    if (child.getExitCode() != 0)
        return false;   // the plugin took the worker down

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

        const auto answer = scan (juce::String (request.value ("format", std::string())),
                                  juce::String (request.value ("file", std::string())));
        if (! writeFrame (pipe, answer))
            return 1;
        if (answer.value ("last", false))
            return answer.value ("ok", false) ? 0 : 1;

        lastAsked = juce::Time::getMillisecondCounter();
    }
}

bool ChildProcessScanner::parseScanEnvelope (const juce::String& output, juce::OwnedArray<juce::PluginDescription>& result)
{
    // the envelope is the last thing on stdout and runs to its end; a plugin may have printed before
    // it, braces included (the worker keeps its own output off stdout, not a plugin's printf): the
    // envelope starts at the first brace from which the rest reads as one
    const auto text = output.toStdString();
    nlohmann::json envelope;
    auto found = false;
    for (auto start = text.find ('{'); start != std::string::npos && ! found; start = text.find ('{', start + 1))
    {
        envelope = nlohmann::json::parse (text.begin() + static_cast<std::string::difference_type> (start), text.end(),
                                          nullptr, false);
        found = envelope.is_object() && envelope.contains ("ok");
    }

    if (! found)
        return false;
    if (! envelope["ok"].is_boolean() || ! envelope["ok"].get<bool>())
        return true;   // the worker ran and declined cleanly (no such format here): nothing in the file, no crash

    if (! envelope.contains ("result") || ! envelope["result"].is_object()
        || ! envelope["result"].contains ("plugins") || ! envelope["result"]["plugins"].is_array())
        return false;

    addDescriptions (envelope["result"]["plugins"], result);
    return true;
}

} // namespace audium
