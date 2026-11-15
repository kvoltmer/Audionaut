//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "ChildProcessScanner.h"

#include <nlohmann/json.hpp>

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

ChildProcessScanner::ChildProcessScanner (juce::File executable_, int timeoutMs_) :
    executable (std::move (executable_)),
    timeoutMs (timeoutMs_)
{
}

bool ChildProcessScanner::findPluginTypesFor (juce::AudioPluginFormat& format,
                                              juce::OwnedArray<juce::PluginDescription>& result,
                                              const juce::String& fileOrIdentifier)
{
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

bool ChildProcessScanner::parseScanEnvelope (const juce::String& output, juce::OwnedArray<juce::PluginDescription>& result)
{
    // the envelope is the last thing on stdout; a plugin may have printed before it
    const auto start = output.indexOfChar ('{');
    if (start < 0)
        return false;

    nlohmann::json envelope;
    try
    {
        envelope = nlohmann::json::parse (output.substring (start).toStdString());
    }
    catch (const nlohmann::json::exception&)
    {
        return false;
    }

    if (! envelope.is_object() || ! envelope.contains ("ok"))
        return false;
    if (! envelope["ok"].is_boolean() || ! envelope["ok"].get<bool>())
        return true;   // the worker ran and declined cleanly (no such format here): nothing in the file, no crash

    if (! envelope.contains ("result") || ! envelope["result"].is_object()
        || ! envelope["result"].contains ("plugins") || ! envelope["result"]["plugins"].is_array())
        return false;

    for (const auto& element : envelope["result"]["plugins"])
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
    return true;
}

} // namespace audium
