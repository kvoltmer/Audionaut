//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include "PluginHost.h"

#include <algorithm>
#include <atomic>
#include <map>
#include <set>
#include <thread>

#include "ChildProcessScanner.h"
#include "HostedEffect.h"
#include "PluginTypeId.h"
#include "Util/Preferences.h"

namespace audium
{

PluginHost::PluginHost (juce::File dataDirectory_, bool withPlatformFormats) :
    dataDirectory (std::move (dataDirectory_))
{
    if (withPlatformFormats)
        addPlatformFormats (formats);
    list.addChangeListener (this);
}

PluginHost::~PluginHost()
{
    list.removeChangeListener (this);
}

juce::File PluginHost::defaultDataDirectory()
{
    return Preferences::applicationDataDirectory();
}

void PluginHost::addPlatformFormats (juce::AudioPluginFormatManager& manager)
{
   #if JUCE_MAC && JUCE_PLUGINHOST_AU
    manager.addFormat (std::make_unique<juce::AudioUnitPluginFormat>());   // the sandbox's one format
   #endif
   #if (JUCE_WINDOWS || JUCE_LINUX || JUCE_BSD) && JUCE_PLUGINHOST_VST3
    manager.addFormat (std::make_unique<juce::VST3PluginFormat>());
   #endif
   #if (JUCE_LINUX || JUCE_BSD) && JUCE_PLUGINHOST_LV2
    manager.addFormat (std::make_unique<juce::LV2PluginFormat>());
   #endif
    juce::ignoreUnused (manager);
}

juce::File PluginHost::getListFile() const
{
    return dataDirectory.getChildFile ("Plugins").getChildFile ("KnownPlugins.xml");
}

juce::File PluginHost::getDeadMansPedalFile() const
{
    return dataDirectory.getChildFile ("Plugins").getChildFile ("ScanInProgress.txt");
}

bool PluginHost::loadList()
{
    const auto file = getListFile();
    if (! file.existsAsFile())
        return false;

    auto xml = juce::parseXML (file);
    if (xml == nullptr)
        return false;

    list.recreateFromXml (*xml);
    // a scan this process did not survive: the file it was on goes on the blacklist
    juce::PluginDirectoryScanner::applyBlacklistingsFromDeadMansPedal (list, getDeadMansPedalFile());
    return true;
}

bool PluginHost::saveList() const
{
    const auto file = getListFile();
    if (! file.getParentDirectory().createDirectory())
        return false;

    auto xml = list.createXml();
    return xml != nullptr && xml->writeTo (file);
}

//==============================================================================
void PluginHost::attach (std::shared_ptr<EffectRegistry> registry, Mode mode)
{
    registerInto (*registry, mode);
    attached.push_back ({ registry, mode });
}

bool PluginHost::canCreate (const juce::PluginDescription& description) const
{
    for (auto* format : formats.getFormats())
        if (format->getName() == description.pluginFormatName)
            return ! format->requiresUnblockedMessageThreadDuringCreation (description);   // AUv3 waits for asynchronous loading
    return false;
}

bool PluginHost::isPluginTypeId (const std::string& typeId) const
{
    for (auto* format : formats.getFormats())
        if (typeId.rfind (PluginTypeId::formatKey (format->getName()) + ":", 0) == 0)
            return true;
    return false;
}

void PluginHost::registerInto (EffectRegistry& registry, Mode mode) const
{
    std::set<std::string> fresh;
    std::map<juce::String, juce::String> formatOfName;   // "name|manufacturer" -> the format registered under the plain name

    // by manufacturer, then name, then format: the menu lists the types in
    // registration order, and the list itself keeps the newest first
    auto descriptions = list.getTypes();
    std::sort (descriptions.begin(), descriptions.end(), [] (const juce::PluginDescription& a, const juce::PluginDescription& b)
    {
        if (const auto byManufacturer = a.manufacturerName.compareIgnoreCase (b.manufacturerName); byManufacturer != 0)
            return byManufacturer < 0;
        if (const auto byName = a.name.compareIgnoreCase (b.name); byName != 0)
            return byName < 0;
        return a.pluginFormatName.compareIgnoreCase (b.pluginFormatName) < 0;
    });

    for (const auto& description : descriptions)
    {
        if (description.isInstrument || ! canCreate (description))
            continue;

        auto info = PluginTypeId::typeInfo (description);
        const auto key = description.name + "|" + description.manufacturerName;
        if (auto it = formatOfName.find (key); it != formatOfName.end() && it->second != description.pluginFormatName)
            info.name += " (" + description.pluginFormatName.toStdString() + ")";   // the same plugin in a second format
        else
            formatOfName[key] = description.pluginFormatName;

        fresh.insert (info.id);
        registry.add (info, [this, description, info, mode] { return createEffect (description, info, mode); });
    }

    // plugins that left the list leave the registry
    const auto types = registry.getTypes();   // a copy: remove() edits the list
    for (const auto& type : types)
        if (isPluginTypeId (type.id) && fresh.count (type.id) == 0)
            registry.remove (type.id);

    registry.setSubstituteResolver ([this] (const json& element) -> std::string
    {
        if (! element.is_object() || ! element.contains ("plugin") || ! element["plugin"].is_object())
            return {};
        const auto& plugin = element["plugin"];
        if (! plugin.contains ("name") || ! plugin["name"].is_string())
            return {};
        const auto manufacturer = plugin.contains ("manufacturer") && plugin["manufacturer"].is_string()
                                    ? plugin["manufacturer"].get<std::string>() : std::string();
        if (auto substitute = findSubstitute (plugin["name"].get<std::string>(), manufacturer))
            return PluginTypeId::make (*substitute);
        return {};
    });
}

std::optional<juce::PluginDescription> PluginHost::findByTypeId (const std::string& typeId) const
{
    for (const auto& description : list.getTypes())
        if (PluginTypeId::make (description) == typeId)
            return description;
    return std::nullopt;
}

std::optional<juce::PluginDescription> PluginHost::findSubstitute (const juce::String& name, const juce::String& manufacturer) const
{
    for (const auto& description : list.getTypes())
    {
        if (description.isInstrument || ! canCreate (description))
            continue;
        if (description.name.trim().equalsIgnoreCase (name.trim())
            && description.manufacturerName.trim().equalsIgnoreCase (manufacturer.trim()))
            return description;
    }
    return std::nullopt;
}

//==============================================================================
std::unique_ptr<juce::AudioPluginInstance> PluginHost::createInstance (const juce::PluginDescription& description,
                                                                       double sampleRate, int blockSize, juce::String& error) const
{
    if (! canCreate (description))
    {
        error = "plugin " + description.name + " (" + description.pluginFormatName + ") cannot be loaded here";
        return nullptr;
    }
    return formats.createPluginInstance (description, sampleRate, blockSize, error);
}

std::unique_ptr<Effect> PluginHost::createEffect (const juce::PluginDescription& description, Mode mode) const
{
    return createEffect (description, PluginTypeId::typeInfo (description), mode);
}

std::unique_ptr<Effect> PluginHost::createEffect (const juce::PluginDescription& description, const EffectTypeInfo& type, Mode mode) const
{
    if (mode == Mode::catalogueOnly)
        return std::make_unique<HostedEffect> (type, description, nullptr, "not loaded in this session");

    // the format the planner prepares with comes later; the plugin is told the real one in prepare()
    juce::String error;
    auto instance = createInstance (description, 44100.0, 512, error);
    if (instance == nullptr && error.isEmpty())
        error = "the plugin could not be loaded";
    return std::make_unique<HostedEffect> (type, description, std::move (instance), error);
}

//==============================================================================
void PluginHost::useChildProcessScanner (juce::File executable, int timeoutMs)
{
    list.setCustomScanner (std::make_unique<ChildProcessScanner> (std::move (executable), timeoutMs));
}

PluginHost::ScanResult PluginHost::scanAll (const std::function<void (float, const juce::String&)>& progress,
                                            const std::function<bool()>& shouldAbort,
                                            const juce::String& onlyFormat)
{
    ScanResult result;
    const auto before = list.getNumTypes();
    const auto blacklistedBefore = list.getBlacklistedFiles().size();
    const auto pedal = getDeadMansPedalFile();
    pedal.getParentDirectory().createDirectory();

    const auto numFormats = formats.getNumFormats();
    for (auto f = 0; f < numFormats; ++f)
    {
        auto* format = formats.getFormat (f);
        if (! format->canScanForPlugins() || (onlyFormat.isNotEmpty() && format->getName() != onlyFormat))
            continue;

        juce::PluginDirectoryScanner::applyBlacklistingsFromDeadMansPedal (list, pedal);
        juce::PluginDirectoryScanner scanner (list, *format, format->getDefaultLocationsToSearch(), true, pedal, true);

        juce::String file;
        auto more = scanner.getNextPluginFileThatWillBeScanned().isNotEmpty();
        while (more)
        {
            more = scanner.scanNextFile (true, file);   // false once the last file is done
            if (progress)
                progress ((static_cast<float> (f) + scanner.getProgress()) / static_cast<float> (numFormats), file);
            if (shouldAbort && shouldAbort())
                break;
        }
        result.failed += scanner.getFailedFiles().size();
    }

    result.found = list.getNumTypes();
    result.added = result.found - before;
    result.blacklisted = list.getBlacklistedFiles().size() - blacklistedBefore;
    saveList();
    return result;
}

bool PluginHost::runWhilePumping (std::function<void()> fn, int timeoutMs)
{
    auto done = std::make_shared<std::atomic<bool>> (false);
    std::thread ([work = std::move (fn), done] { work(); done->store (true); }).detach();

    const auto deadline = juce::Time::getMillisecondCounter() + static_cast<juce::uint32> (timeoutMs);
    auto* messageManager = juce::MessageManager::getInstanceWithoutCreating();
    while (! done->load())
    {
        if (juce::Time::getMillisecondCounter() >= deadline)
            return false;
        if (messageManager != nullptr && messageManager->isThisTheMessageThread())
            messageManager->runDispatchLoopUntil (20);
        else
            juce::Thread::sleep (20);
    }
    return true;
}

//==============================================================================
void PluginHost::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // message thread: the list changed (a scan, a removal, the blacklist)
    saveList();

    attached.erase (std::remove_if (attached.begin(), attached.end(),
                                    [] (const Attached& entry) { return entry.registry.expired(); }),
                    attached.end());
    for (const auto& entry : attached)
        if (auto registry = entry.registry.lock())
            registerInto (*registry, entry.mode);
}

} // namespace audium
