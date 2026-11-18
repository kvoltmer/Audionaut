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

        // only when the app that asked allows it: otherwise the system would ask this worker's
        // (absent) user to lower the security settings, and such a plugin simply fails to load
        PluginFormats::applyLoweredSecurity (formats, request.loweredSecurity);

        auto* format = formatNamed (formatName);
        if (format == nullptr)
            return { { "ok", false }, { "code", "unknown_format" },
                     { "message", "this build hosts no " + formatName.toStdString() + " plugins" } };

        // The scan's work is shared with its thread, not lent: a plugin that is still loading when the
        // time is up goes on using the format and the result after this function has returned.
        struct Scan
        {
            juce::AudioPluginFormat* format = nullptr;
            juce::String fileOrIdentifier;
            juce::OwnedArray<juce::PluginDescription> found;
            bool asksUser = false;
            juce::String error;
        };
        auto work = std::make_shared<Scan>();
        work->format = format;
        work->fileOrIdentifier = fileOrIdentifier;
        // a plugin the system asks the user about before it loads it here
        work->asksUser = request.loweredSecurity && PluginFormats::asksUserBeforeLoading (formatName, fileOrIdentifier);

        // the plugin is loaded to read its description: on a thread of its own,
        // the message thread pumping, since some formats refuse to load on it
        const auto load = [work]
        {
            if (! work->asksUser)
            {
                work->format->findAllTypesForFile (work->found, work->fileOrIdentifier);
                return;
            }

            // such a plugin is one type, read off its instance - made here, for the reason it gives
            // when the system does not load it
            juce::PluginDescription wanted;
            wanted.pluginFormatName = work->format->getName();
            wanted.fileOrIdentifier = work->fileOrIdentifier;
            if (auto instance = work->format->createInstanceFromDescription (wanted, 44100.0, 512, work->error))
                work->found.add (new juce::PluginDescription (instance->getPluginDescription()));
        };

        if (! PluginFormats::runWhilePumping (load, request.timeoutMs))
        {
            stuck = true;

            // A plugin the system asks the user about may have waited for that answer all the time, not
            // for itself: the scanner is told, and does not hold the first such wait against the file.
            if (work->asksUser)
                return { { "ok", false }, { "code", "awaiting_consent" },
                         { "message", "the plugin did not finish loading; macOS may have been waiting for the answer about its security settings" },
                         { "last", true } };

            return { { "ok", false }, { "code", "timeout" }, { "message", "the plugin did not finish loading" }, { "last", true } };
        }

        // The system's own word that the user said no to its question (userCanceledErr, in the reason the
        // format gives): the scanner is told, so that it does not have it raised for every further plugin.
        if (work->asksUser && work->found.isEmpty() && work->error.contains ("(-128)"))
            return { { "ok", false }, { "code", "consent_declined" },
                     { "message", "macOS did not load the plugin, the question about the security settings was declined" } };

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
