//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "Cli/Commands/Commands.h"
#include "Cli/HeadlessEngineSession.h"
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

} // namespace

int runScanPlugin (const juce::ArgumentList& args, CliContext& context)
{
    auto working = args;
    const auto formatName = takeOptionValue (working, "--format");
    ScopedCoutToStderr guard (context.json);   // whatever the plugin prints while loading stays off the envelope

    if (formatName.isEmpty())
        return context.fail (exitUsage, "usage", "scan-plugin requires --format <name>");
    if (working.size() < 2)
        return context.fail (exitUsage, "usage", "scan-plugin requires the plugin file or identifier");
    const auto fileOrIdentifier = working[1].text;

    juce::AudioPluginFormatManager formats;
    PluginHost::addPlatformFormats (formats);
    juce::AudioPluginFormat* format = nullptr;
    for (auto* candidate : formats.getFormats())
        if (candidate->getName() == formatName)
            format = candidate;
    if (format == nullptr)
        return context.fail (exitUnavailable, "unknown_format", "this build hosts no " + formatName.toStdString() + " plugins");

    // the plugin is loaded to read its description: on a thread of its own,
    // the message thread pumping, since some formats refuse to load on it
    ScopedMessageLoop messageLoop;
    juce::OwnedArray<juce::PluginDescription> found;
    const auto finished = PluginHost::runWhilePumping ([&] { format->findAllTypesForFile (found, fileOrIdentifier); }, 50000);
    if (! finished)
        return context.fail (exitFailure, "timeout", "the plugin did not finish loading");

    auto plugins = nlohmann::json::array();
    for (const auto* description : found)
        if (auto xml = description->createXml())
            plugins.push_back (xml->toString().toStdString());

    context.log (juce::String (found.size()) + " plugin(s) in " + fileOrIdentifier);
    return context.ok ({ { "plugins", plugins } });
}

int runScanPlugins (const juce::ArgumentList& args, CliContext& context)
{
    auto working = args;
    const auto formatName = takeOptionValue (working, "--format");
    ScopedCoutToStderr guard (context.json);

    if (context.pluginHost == nullptr)
        return context.fail (exitUnavailable, "feature_unavailable", "this process hosts no plugins");

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
