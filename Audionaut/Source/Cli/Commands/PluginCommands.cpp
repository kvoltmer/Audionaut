//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Cli/Commands/Commands.h"
#include "Cli/HeadlessEngineSession.h"
#include "Engine/Effects/Hosting/ChildProcessScanner.h"
#include "Engine/Effects/Hosting/PluginHost.h"

namespace audium {
namespace cli {

namespace {

/**
 * A message loop for a verb that opens no engine: the worker's plugin call
 * runs off the message thread and wants it pumped. Owns the MessageManager
 * unless the process already runs one (the app's in-app CLI).
 */
struct ScopedMessageLoop
{
    ScopedMessageLoop() :
        owns (! HeadlessEngineSession::usesExternalMessageManager())
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
struct ScanWorker
{
    ScanWorker() { PluginHost::addPlatformFormats (formats); }

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

    /// One file: `{"ok": true, "plugins": [xml, ...]}`, or `{"ok": false, "code", "message"}` - a
    /// timeout among them, which is this worker's last answer (`"last": true`).
    nlohmann::json scan (const ChildProcessScanner::Request& request)
    {
        const auto& formatName = request.format;
        const auto& fileOrIdentifier = request.file;

        // only when the app that asked allows it: otherwise the system would ask this worker's
        // (absent) user to lower the security settings, and such a plugin simply fails to load
        PluginHost::applyLoweredSecurity (formats, request.loweredSecurity);

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
        };
        auto work = std::make_shared<Scan>();
        work->format = format;
        work->fileOrIdentifier = fileOrIdentifier;

        // the plugin is loaded to read its description: on a thread of its own,
        // the message thread pumping, since some formats refuse to load on it
        if (messageLoop == nullptr)
            messageLoop = std::make_unique<ScopedMessageLoop>();
        if (! PluginHost::runWhilePumping ([work] { work->format->findAllTypesForFile (work->found, work->fileOrIdentifier); }, request.timeoutMs))
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

    juce::AudioPluginFormatManager formats;
    std::unique_ptr<ScopedMessageLoop> messageLoop;
    bool stuck = false;
};

} // namespace

int runScanPlugin (const juce::ArgumentList& args, CliContext& context)
{
    auto working = args;
    const auto formatName = takeOptionValue (working, "--format");
    const auto pipeName = takeOptionValue (working, "--serve");
    const auto timeoutText = takeOptionValue (working, "--timeout-ms");
    const auto timeoutMs = timeoutText.getIntValue() > 0 ? timeoutText.getIntValue() : 50000;
    const auto loweredSecurity = working.removeOptionIfFound ("--allow-lowered-security");
    ScopedCoutToStderr guard (context.json || pipeName.isNotEmpty());   // whatever a plugin prints while loading stays off the envelope

    // A thread still inside a plugin when the time was up goes on using the formats and wants the
    // message loop: the worker stays then. This process is a scan worker and ends with the verb;
    // whoever started it takes the failure as the verdict.
    auto* worker = new ScanWorker();
    const auto finish = [worker] (int exitCode)
    {
        if (! worker->stuck)
            delete worker;
        return exitCode;
    };

    // many files, one after the other, for a scanner at the other end of a pipe
    if (pipeName.isNotEmpty())
        return finish (ChildProcessScanner::serve (pipeName,
                                                   [worker] (const ChildProcessScanner::Request& request) { return worker->scan (request); },
                                                   5000,
                                                   [worker] { worker->pump(); }));

    if (formatName.isEmpty())
        return finish (context.fail (exitUsage, "usage", "scan-plugin requires --format <name>"));
    if (working.size() < 2)
        return finish (context.fail (exitUsage, "usage", "scan-plugin requires the plugin file or identifier"));
    const auto fileOrIdentifier = working[1].text;

    const auto answer = worker->scan ({ formatName, fileOrIdentifier, timeoutMs, loweredSecurity });
    if (! answer.value ("ok", false))
    {
        const auto code = answer.value ("code", std::string ("failed"));
        return finish (context.fail (code == "unknown_format" ? exitUnavailable : exitFailure, code,
                                     answer.value ("message", std::string())));
    }

    context.log (juce::String (static_cast<int> (answer["plugins"].size())) + " plugin(s) in " + fileOrIdentifier);
    return finish (context.ok ({ { "plugins", answer["plugins"] } }));
}

int runScanPlugins (const juce::ArgumentList& args, CliContext& context)
{
    auto working = args;
    const auto formatName = takeOptionValue (working, "--format");
    ScopedCoutToStderr guard (context.json);

    if (context.pluginHost == nullptr)
        return context.fail (exitUnavailable, "feature_unavailable", "this process hosts no plugins");
    if (! context.pluginHost->isEnabled())
        return context.fail (exitUnavailable, "plugins_disabled", "plugins are turned off: turn them on in Audionaut's Manage Plugins window");

    auto& formats = context.pluginHost->getFormats();
    if (formatName.isNotEmpty())
    {
        auto known = false;
        for (auto* format : formats.getFormats())
            known = known || format->getName() == formatName;
        if (! known)
            return context.fail (exitUnavailable, "unknown_format", "this build hosts no " + formatName.toStdString() + " plugins");
    }

    ScopedMessageLoop messageLoop;   // the list reports its changes through it
    juce::StringArray formatNames;
    for (auto* format : formats.getFormats())
        if (formatName.isEmpty() || format->getName() == formatName)
            formatNames.add (format->getName());

    const auto result = context.pluginHost->scanAll (
        [&] (float fraction, const juce::String& file) { context.progress (fraction, "scanning " + file); },
        {},
        formatName);

    auto formatList = nlohmann::json::array();
    for (const auto& name : formatNames)
        formatList.push_back (name.toStdString());

    context.log (juce::String (result.found) + " plugins known, " + juce::String (result.added) + " new, "
                 + juce::String (result.failed) + " files without a plugin, " + juce::String (result.blacklisted) + " blacklisted");
    return context.ok ({ { "found", result.found },
                         { "added", result.added },
                         { "failed", result.failed },
                         { "blacklisted", result.blacklisted },
                         { "formats", formatList },
                         { "list", context.pluginHost->getListFile().getFullPathName().toStdString() } });
}

} // namespace cli
} // namespace audium
