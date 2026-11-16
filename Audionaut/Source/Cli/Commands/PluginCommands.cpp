//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Cli/Commands/Commands.h"
#include "Cli/HeadlessEngineSession.h"
#include "Engine/Effects/Hosting/ChildProcessScanner.h"
#include "Engine/Effects/Hosting/PluginHost.h"
#include "Engine/Effects/Hosting/ScanWorker.h"

namespace audium {
namespace cli {

namespace {

/**
 * A message loop for a verb that opens no engine: the plugin list reports
 * its changes through it. Owns the MessageManager unless the process
 * already runs one (the app's in-app CLI).
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

    const auto ownsMessageManager = ! HeadlessEngineSession::usesExternalMessageManager();

    // many files, one after the other, for a scanner at the other end of a pipe
    if (pipeName.isNotEmpty())
        return ScanWorker::serve (pipeName, ownsMessageManager);

    if (formatName.isEmpty())
        return context.fail (exitUsage, "usage", "scan-plugin requires --format <name>");
    if (working.size() < 2)
        return context.fail (exitUsage, "usage", "scan-plugin requires the plugin file or identifier");
    const auto fileOrIdentifier = working[1].text;

    const auto answer = ScanWorker::scanOneFile ({ formatName, fileOrIdentifier, timeoutMs, loweredSecurity }, ownsMessageManager);
    if (! answer.value ("ok", false))
    {
        const auto code = answer.value ("code", std::string ("failed"));
        return context.fail (code == "unknown_format" ? exitUnavailable : exitFailure, code,
                             answer.value ("message", std::string()));
    }

    context.log (juce::String (static_cast<int> (answer["plugins"].size())) + " plugin(s) in " + fileOrIdentifier);
    return context.ok ({ { "plugins", answer["plugins"] } });
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
