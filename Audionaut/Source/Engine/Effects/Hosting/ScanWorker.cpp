//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "ScanWorker.h"

#include <memory>

#include "PluginFormats.h"

namespace audium
{

namespace
{

/**
 * A message loop for a process that opens no engine: the worker's plugin
 * call runs off the message thread and wants it pumped. Owns the
 * MessageManager unless the process already runs one.
 */
struct ScopedMessageLoop
{
    explicit ScopedMessageLoop (bool owns_) :
        owns (owns_)
    {
        if (owns)
            juce::MessageManager::getInstance();
    }

    ~ScopedMessageLoop()
    {
        if (owns) {
            juce::DeletedAtShutdown::deleteAll();
            juce::MessageManager::deleteInstance();
        }
    }

    const bool owns;
};

/// The formats this process scans with, and the loop its plugins load under.
struct Worker
{
    explicit Worker (bool ownsMessageManager) :
        messageLoop (ownsMessageManager)
    {
        PluginFormats::addPlatformFormats (formats);
    }

    juce::AudioPluginFormat* formatNamed (const juce::String& name) const
    {
        for (auto* candidate : formats.getFormats())
            if (candidate->getName() == name)
                return candidate;
        return nullptr;
    }

    void pump() const
    {
        if (auto* messageManager = juce::MessageManager::getInstanceWithoutCreating();
            messageManager != nullptr && messageManager->isThisTheMessageThread())
            messageManager->runDispatchLoopUntil (1);
    }

    /// One file; a timeout is this worker's last answer (`"last": true`).
    nlohmann::json scan (const ChildProcessScanner::Request& request)
    {
        const auto& formatName = request.format;
        const auto& fileOrIdentifier = request.file;

        auto* format = formatNamed (formatName);
        if (format == nullptr)
            return { { "ok", false }, { "code", "unknown_format" },
                     { "message", "this build hosts no " + formatName.toStdString() + " plugins" } };

        // A plugin the system asks the user about is the app's to load: the scan helper has not the
        // entitlement the system opens it for, and where the app's own binary is the worker (a command
        // run without the app) nobody is there to answer. Said, not tried: it would look like an empty file.
        if (PluginFormats::asksUserBeforeLoading (formatName, fileOrIdentifier))
            return { { "ok", false }, { "code", "needs_app" },
                     { "message", "macOS only loads this plugin into the app itself, with lowered security settings" } };

        // The scan's work is shared with its thread, not lent: a plugin that is still loading when the
        // time is up goes on using the format and the result after this function has returned.
        struct Scan
        {
            juce::AudioPluginFormat* format = nullptr;
            juce::String fileOrIdentifier;
            juce::OwnedArray<juce::PluginDescription> found;
        };
        auto work = std::make_shared<Scan>();
        work->format = format;
        work->fileOrIdentifier = fileOrIdentifier;

        // the plugin is loaded to read its description: on a thread of its own,
        // the message thread pumping, since some formats refuse to load on it
        const auto load = [work] { work->format->findAllTypesForFile (work->found, work->fileOrIdentifier); };

        if (! PluginFormats::runWhilePumping (load, request.timeoutMs))
        {
            stuck = true;
            return { { "ok", false }, { "code", "timeout" }, { "message", "the plugin did not finish loading" }, { "last", true } };
        }

        auto plugins = nlohmann::json::array();
        for (const auto* description : work->found)
            if (auto xml = description->createXml())
                plugins.push_back (xml->toString().toStdString());
        return { { "ok", true }, { "plugins", plugins } };
    }

    ScopedMessageLoop messageLoop;   // before the formats, which listen to it
    juce::AudioPluginFormatManager formats;
    bool stuck = false;
};

/// A worker that got stuck stays: the plugin's thread still uses its formats and its message loop.
void release (Worker* worker)
{
    if (! worker->stuck)
        delete worker;
}

} // namespace

int ScanWorker::serve (const juce::String& pipeName, bool ownsMessageManager)
{
    auto* worker = new Worker (ownsMessageManager);
    const auto exitCode = ChildProcessScanner::serve (pipeName,
                                                      [worker] (const ChildProcessScanner::Request& request) { return worker->scan (request); },
                                                      5000,
                                                      [worker] { worker->pump(); });
    release (worker);
    return exitCode;
}

nlohmann::json ScanWorker::scanOneFile (const ChildProcessScanner::Request& request, bool ownsMessageManager)
{
    auto* worker = new Worker (ownsMessageManager);
    auto answer = worker->scan (request);
    release (worker);
    return answer;
}

} // namespace audium
