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
#include <utility>

#include "AudioUnitRegistry.h"
#include "ChildProcessScanner.h"
#include "HostedEffect.h"
#include "PluginFormats.h"
#include "PluginTypeId.h"
#include "Util/Preferences.h"

namespace audium
{

PluginHost::PluginHost (juce::File dataDirectory_, bool withPlatformFormats) :
    dataDirectory (std::move (dataDirectory_))
{
    if (withPlatformFormats)
    {
        addPlatformFormats (formats);

       #if JUCE_MAC && JUCE_PLUGINHOST_AU
        // The plugins the system asks about are those that only load out of process because this
        // process is sandboxed: what a format that isolates them would load that way, less what
        // runs out of process anyway (AUv3).
        auto isolatingUnits = std::make_shared<juce::AudioUnitPluginFormat>();
        isolatingUnits->setProcessIsolation (juce::AudioUnitPluginFormat::ProcessIsolation::whenSandboxRequires);
        const std::shared_ptr<juce::AudioPluginFormat> isolating = isolatingUnits;
        const std::shared_ptr<juce::AudioPluginFormat> plain = std::make_shared<juce::AudioUnitPluginFormat>();
        asksForLoweredSecurity = [isolating, plain] (const juce::PluginDescription& description)
        {
            return description.pluginFormatName == plain->getName()
                && isolating->requiresUnblockedMessageThreadDuringCreation (description)
                && ! plain->requiresUnblockedMessageThreadDuringCreation (description);
        };
       #endif

        // empty where the system has no list of its plugins
        installedSource = [this] { return AudioUnitRegistry::installed (formats); };
    }
    list.addChangeListener (this);

    // where there is no list file yet (or none that reads), the empty list is what this host started
    // with: unchanged, it is not written over a file another process made meanwhile
    persisted = listAsText();
}

PluginHost::~PluginHost()
{
    list.removeChangeListener (this);
    questions->closed = true;   // a load still under way finds nothing more to start
    checks->hostAlive.reset();
    checks->pool.removeAllJobs (true, 60000);   // a check under way gives up its plugin
}

bool PluginHost::syncWithInstalled()
{
    if (! installedSource)
        return false;

    const auto installed = installedSource();
    const auto isInstalled = [&installed] (const juce::PluginDescription& description)
    {
        return std::any_of (installed.begin(), installed.end(), [&description] (const juce::PluginDescription& other)
        {
            return other.pluginFormatName == description.pluginFormatName && other.fileOrIdentifier == description.fileOrIdentifier;
        });
    };

    auto changed = false;
    for (const auto& description : list.getTypes())   // a copy
    {
        if (! isScanned (description) && ! isInstalled (description))
        {
            list.removeType (description);
            changed = true;
        }
    }

    const auto skipped = list.getBlacklistedFiles();   // a plugin a scan gave up on stays skipped
    for (const auto& description : installed)
        if (! skipped.contains (description.fileOrIdentifier) && list.getTypeForFile (description.fileOrIdentifier) == nullptr)
            changed = list.addType (description) || changed;
    return changed;
}

void PluginHost::prepareScan()
{
    if (! installedSource)
        return;

    for (const auto& description : list.getTypes())   // a copy
        if (! isScanned (description))
            list.removeType (description);
}

void PluginHost::finishScan()
{
    syncWithInstalled();
}

juce::File PluginHost::defaultDataDirectory()
{
    return Preferences::applicationDataDirectory();
}

void PluginHost::addPlatformFormats (juce::AudioPluginFormatManager& manager)
{
    PluginFormats::addPlatformFormats (manager);
}

void PluginHost::applyLoweredSecurity (juce::AudioPluginFormatManager& manager, bool allowed)
{
    PluginFormats::applyLoweredSecurity (manager, allowed);
}

bool PluginHost::isSandboxed()
{
   #if JUCE_MAC
    return juce::SystemStats::getEnvironmentVariable ("APP_SANDBOX_CONTAINER_ID", {}).isNotEmpty();
   #else
    return false;
   #endif
}

void PluginHost::setEnabled (bool enabled_)
{
    if (std::exchange (enabled, enabled_) == enabled_)
        return;

    registerIntoAttached();   // the plugins come, or go
}

void PluginHost::setLoweredSecurityAllowed (bool allowed)
{
    if (loweredSecurityAllowed->exchange (allowed) == allowed)
        return;

    applyLoweredSecurity (formats, allowed);
    registerIntoAttached();   // the plugins in question come, or go
}

void PluginHost::setEnabled (bool enabled_, bool loweredSecurityAllowed_)
{
    const auto enabledChanged = std::exchange (enabled, enabled_) != enabled_;
    const auto allowedChanged = loweredSecurityAllowed->exchange (loweredSecurityAllowed_) != loweredSecurityAllowed_;
    if (allowedChanged)
        applyLoweredSecurity (formats, loweredSecurityAllowed_);
    if (enabledChanged || allowedChanged)
        registerIntoAttached();
}

juce::File PluginHost::getListFile() const
{
    return dataDirectory.getChildFile ("Plugins").getChildFile ("KnownPlugins.xml");
}

juce::File PluginHost::getDeadMansPedalFile() const
{
    return dataDirectory.getChildFile ("Plugins").getChildFile ("ScanInProgress.txt");
}

juce::File PluginHost::getScanLogFile() const
{
    return dataDirectory.getChildFile ("Plugins").getChildFile ("ScanLog.txt");
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
    persisted = listAsText();   // before the pedal's verdicts: those are news the file does not have
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
    if (xml == nullptr || ! xml->writeTo (file))
        return false;

    persisted = xml->toString();
    return true;
}

bool PluginHost::saveListIfChanged() const
{
    return listAsText() == persisted || saveList();
}

juce::String PluginHost::listAsText() const
{
    auto xml = list.createXml();
    return xml != nullptr ? xml->toString() : juce::String();
}

//==============================================================================
void PluginHost::attach (std::shared_ptr<EffectRegistry> registry, Mode mode)
{
    registerInto (*registry, mode);
    attached.push_back ({ registry, mode });
}

PluginHost::Availability PluginHost::availabilityOf (const juce::PluginDescription& description) const
{
    if (formatOf (description) == nullptr)
        return Availability::formatMissing;
    if (! enabled)
        return Availability::disabled;
    if (! isLoweredSecurityAllowed() && asksForLoweredSecurity && asksForLoweredSecurity (description))
        return Availability::needsLoweredSecurity;
    return Availability::available;
}

juce::String PluginHost::describe (Availability availability)
{
    switch (availability)
    {
        case Availability::available:      return {};
        case Availability::formatMissing:  return TRANS ("its format is not hosted on this platform");
        case Availability::disabled:       return TRANS ("plugins are turned off; Manage Plugins turns them on");
        case Availability::needsLoweredSecurity:
            return TRANS ("macOS only loads it with lowered security settings, which a command run without the app cannot ask for");
    }
    return {};
}

juce::String PluginHost::describeLoadError (const juce::String& message)
{
    // userCanceledErr: the user said no when the system asked to lower the security settings (or
    // nobody was there to say yes)
    if (message.contains ("(-128)"))
        return TRANS ("macOS did not load the plugin: lowering the security settings for it was declined");
    return message;
}

bool PluginHost::canCreate (const juce::PluginDescription& description) const
{
    return availabilityOf (description) == Availability::available;
}

juce::AudioPluginFormat* PluginHost::formatOf (const juce::PluginDescription& description) const
{
    for (auto* format : formats.getFormats())
        if (format->getName() == description.pluginFormatName)
            return format;
    return nullptr;
}

bool PluginHost::loadsOutOfProcess (const juce::PluginDescription& description) const
{
    auto* format = formatOf (description);
    return format != nullptr && format->requiresUnblockedMessageThreadDuringCreation (description);
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
        registry.add (info, [this, description, info, mode, arrivals = registry.getPendingArrivals()]
        {
            return createEffect (description, info, mode, arrivals);
        });
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
    auto instance = formats.createPluginInstance (description, sampleRate, blockSize, error);
    error = describeLoadError (error);
    return instance;
}

std::unique_ptr<Effect> PluginHost::createEffect (const juce::PluginDescription& description, Mode mode) const
{
    return createEffect (description, PluginTypeId::typeInfo (description), mode);
}

std::unique_ptr<Effect> PluginHost::createEffect (const juce::PluginDescription& description, const EffectTypeInfo& type, Mode mode,
                                                  std::shared_ptr<std::atomic<int>> arrivalCount) const
{
    if (mode == Mode::catalogueOnly)
    {
        auto effect = std::make_unique<HostedEffect> (type, description, nullptr, "not loaded in this session");
        effect->setArrivalCount (std::move (arrivalCount));   // for a load() later
        return effect;
    }

    // a plugin nobody has loaded yet, which would run in here: the scan worker loads it first
    if (needsCheckBeforeLoad (description))
    {
        auto effect = std::make_unique<HostedEffect> (type, description, HostedEffect::Loading {});
        effect->setArrivalCount (std::move (arrivalCount));
        requestChecked (*effect);
        return effect;
    }

    // the format the planner prepares with comes later; the plugin is told the real one in prepare()
    if (loadsOutOfProcess (description))
    {
        // the plugin comes through the message queue: a placeholder stands in until then
        auto effect = std::make_unique<HostedEffect> (type, description, HostedEffect::Loading {});
        effect->setArrivalCount (std::move (arrivalCount));
        requestInstance (*effect);
        return effect;
    }

    juce::String error;
    auto instance = createInstance (description, 44100.0, 512, error);
    if (instance == nullptr && error.isEmpty())
        error = "the plugin could not be loaded";
    auto effect = std::make_unique<HostedEffect> (type, description, std::move (instance), error);
    effect->setArrivalCount (std::move (arrivalCount));
    return effect;
}

void PluginHost::requestInstance (HostedEffect& effect) const
{
    // counted for the host and for the effect's registry (its engine), which may be all a caller waits for
    const auto arrivals = effect.getArrivalCount();
    ++*pendingLoads;
    if (arrivals != nullptr)
        ++*arrivals;

    auto* format = formatOf (effect.getDescription());

    // A plugin the system asks the user about waits its turn: one question at a time, and a "no"
    // answers for those waiting behind it.
    if (asksForLoweredSecurity && asksForLoweredSecurity (effect.getDescription()))
    {
        questions->waiting.push_back ({ effect.getAliveToken(), &effect, format, arrivals });
        if (! questions->busy)
            loadNextAsked (questions, pendingLoads);
        return;
    }

    format->createPluginInstanceAsync (effect.getDescription(), 44100.0, 512,
                                       [alive = effect.getAliveToken(), waiting = &effect, pending = pendingLoads, arrivals]
                                       (std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String& message)
                                       {
                                           // message thread; the effect may be gone by now (an undo, a closed
                                           // project): the plugin goes away with the callback then
                                           if (alive.lock() != nullptr)
                                               waiting->adopt (std::move (instance), describeLoadError (message));
                                           --*pending;
                                           if (arrivals != nullptr)
                                               --*arrivals;
                                       });
}

void PluginHost::loadNextAsked (std::shared_ptr<Questions> questions, std::shared_ptr<std::atomic<int>> pending)
{
    // an effect that left meanwhile (an undo, a closed project) has nothing to load; after the host
    // nothing has
    const auto settle = [pending] (const Questions::Waiting& one, std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String& message)
    {
        if (one.alive.lock() != nullptr)
            one.effect->adopt (std::move (instance), describeLoadError (message));
        --*pending;
        if (one.arrivals != nullptr)
            --*one.arrivals;
    };

    while (! questions->waiting.empty())
    {
        const auto next = questions->waiting.front();
        questions->waiting.pop_front();
        if (questions->closed || next.alive.lock() == nullptr)
        {
            settle (next, nullptr, {});
            continue;
        }

        questions->busy = true;
        next.format->createPluginInstanceAsync (next.effect->getDescription(), 44100.0, 512,
                                                [questions, pending, next, settle]
                                                (std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String& message)
                                                {
                                                    // message thread
                                                    const auto declined = instance == nullptr && message.contains ("(-128)");   // userCanceledErr
                                                    settle (next, std::move (instance), message);

                                                    // the user said no: not asked again for each plugin that waited meanwhile
                                                    if (declined)
                                                    {
                                                        for (const auto& other : std::exchange (questions->waiting, {}))
                                                            settle (other, nullptr, message);
                                                    }

                                                    questions->busy = false;
                                                    loadNextAsked (questions, pending);
                                                });
        return;
    }
    questions->busy = false;
}

bool PluginHost::needsCheckBeforeLoad (const juce::PluginDescription& description) const
{
    // a plugin in a process of its own takes nothing down with it
    if (! checksBeforeFirstLoad || loadsOutOfProcess (description))
        return false;

    const auto listed = list.getTypeForFile (description.fileOrIdentifier);   // as the list has it now
    return listed != nullptr && ! isScanned (*listed);
}

void PluginHost::requestChecked (HostedEffect& effect) const
{
    const auto arrivals = effect.getArrivalCount();
    ++*pendingLoads;
    if (arrivals != nullptr)
        ++*arrivals;

    checks->pool.addJob ([this, &known = checks->list, format = formatOf (effect.getDescription()), description = effect.getDescription(),
                          hostAlive = std::weak_ptr<int> (checks->hostAlive), alive = effect.getAliveToken(), waiting = &effect,
                          pending = pendingLoads, arrivals]
    {
        // the list's scanner loads it (in the worker process), and the list has the scan's word after
        juce::OwnedArray<juce::PluginDescription> found;
        known.scanAndAddFile (description.fileOrIdentifier, false, found, *format);

        juce::MessageManager::callAsync ([this, &known, description, hostAlive, alive, waiting, pending, arrivals, loads = ! found.isEmpty()]
        {
            std::unique_ptr<juce::AudioPluginInstance> instance;
            juce::String error ("the plugin could not be loaded");

            if (hostAlive.lock() != nullptr)   // else the list and the formats are gone
            {
                if (known.getBlacklistedFiles().contains (description.fileOrIdentifier))
                {
                    // it took the worker down, or too long: skipped from now on, not listed any more
                    for (const auto& listed : known.getTypes())
                        if (listed.fileOrIdentifier == description.fileOrIdentifier && ! isScanned (listed))
                            known.removeType (listed);
                    error = TRANS ("the plugin crashed or took too long when it was checked before its first load, and is skipped from now on");
                }
                else if (loads && alive.lock() != nullptr)
                {
                    instance = createInstance (description, 44100.0, 512, error);
                }
                known.sendChangeMessage();   // the entry is a scanned one now, or gone
            }

            if (alive.lock() != nullptr)
                waiting->adopt (std::move (instance), error);
            --*pending;
            if (arrivals != nullptr)
                --*arrivals;
        });
    });
}

bool PluginHost::load (HostedEffect& effect) const
{
    if (effect.isLoaded() || effect.getStatus() == HostedEffect::Status::loading)
        return true;

    if (! canCreate (effect.getDescription()))
        return false;

    effect.expectPlugin();
    if (needsCheckBeforeLoad (effect.getDescription()))
    {
        requestChecked (effect);
        return true;
    }
    if (loadsOutOfProcess (effect.getDescription()))
    {
        requestInstance (effect);
        return true;
    }

    juce::String error;
    auto instance = createInstance (effect.getDescription(), 44100.0, 512, error);
    effect.adopt (std::move (instance), error);
    return effect.isLoaded();
}

bool PluginHost::waitForPendingLoads (int timeoutMs) const
{
    return waitUntilNone (*pendingLoads, timeoutMs);
}

bool PluginHost::waitForPendingLoads (const EffectRegistry& registry, int timeoutMs)
{
    return waitUntilNone (*registry.getPendingArrivals(), timeoutMs);
}

bool PluginHost::waitUntilNone (const std::atomic<int>& pending, int timeoutMs)
{
    const auto deadline = juce::Time::getMillisecondCounter() + static_cast<juce::uint32> (timeoutMs);
    auto* messageManager = juce::MessageManager::getInstanceWithoutCreating();
    while (pending.load() > 0)
    {
        if (juce::Time::getMillisecondCounter() >= deadline)
            return false;
        if (messageManager != nullptr && messageManager->isThisTheMessageThread())
        {
            // a loop that was told to stop (the app is quitting) delivers nothing any more and returns
            // at once: waiting on would spin until the deadline
            if (! messageManager->runDispatchLoopUntil (20))
                return false;
        }
        else
        {
            juce::Thread::sleep (20);
        }
    }
    return true;
}

//==============================================================================
void PluginHost::useChildProcessScanner (juce::File executable, int timeoutMs)
{
    const auto workerPath = executable.getFullPathName();
    auto scanner = std::make_unique<ChildProcessScanner> (std::move (executable), timeoutMs);
    // The worker loads what this host may load. Only a sandboxed app is ever asked about a plugin; anywhere
    // else the flag changes nothing in the worker, and the scanner need not start new workers over it.
    if (isSandboxed())
        scanner->setLoweredSecurityFlag (loweredSecurityAllowed);
    checksBeforeFirstLoad = true;   // a plugin that crashes does so in the worker

    // The scan's log, for a user to send along when a scan goes wrong: a heading whenever the scanner
    // takes up work again after a pause (a new scan), then its lines with the time of day.
    struct Log
    {
        juce::File file;
        juce::String worker;
        std::shared_ptr<const std::atomic<bool>> loweredSecurity;
        juce::Time last;
    };
    auto state = std::make_shared<Log> (Log { getScanLogFile(), workerPath, loweredSecurityAllowed, {} });
    scanner->setLog ([state] (const juce::String& line)
    {
        const auto now = juce::Time::getCurrentTime();
        juce::String text;

        if ((now - state->last).inSeconds() > 20.0)
        {
            if (state->file.getSize() > 1024 * 1024)
                state->file.deleteFile();   // the old scans go, this one is kept whole
            state->file.getParentDirectory().createDirectory();

            text << (state->file.existsAsFile() ? "\n" : "")
                 << "=== Plugin scan, " << now.toString (true, true) << " ===\n"
                 << "Audionaut " << ProjectInfo::versionString
                #if JUCE_DEBUG
                 << " (debug build)"
                #endif
                 << ", " << juce::SystemStats::getOperatingSystemName()
                 << (juce::SystemStats::isOperatingSystem64Bit() ? ", 64 bit" : ", 32 bit")
                 << ", " << juce::SystemStats::getCpuModel() << "\n"
                 << "Sandboxed: " << (isSandboxed() ? "yes" : "no")
                 << ", lowered security settings allowed: " << (state->loweredSecurity->load() ? "yes" : "no") << "\n"
                 << "Worker: " << state->worker << "\n";
        }
        state->last = now;

        text << now.formatted ("%H:%M:%S") << "  " << line << "\n";
        state->file.appendText (text, false, false, "\n");
    });
    list.setCustomScanner (std::move (scanner));
}

juce::File PluginHost::scanWorkerExecutable()
{
   #if JUCE_MAC
    // Contents/Helpers, where the Xcode project's post-build step puts it (ScanHelper/embed-scan-helper.sh)
    const auto helper = juce::File::getSpecialLocation (juce::File::currentApplicationFile)
                            .getChildFile ("Contents/Helpers/audionaut-scan-helper");
    if (helper.existsAsFile())
        return helper;
   #endif
    return juce::File::getSpecialLocation (juce::File::currentExecutableFile);
}

PluginHost::ScanResult PluginHost::scanAll (const std::function<void (float, const juce::String&)>& progress,
                                            const std::function<bool()>& shouldAbort,
                                            const juce::String& onlyFormat)
{
    ScanResult result;
    prepareScan();
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

    result.added = list.getNumTypes() - before;
    finishScan();
    result.found = list.getNumTypes();
    result.blacklisted = list.getBlacklistedFiles().size() - blacklistedBefore;
    saveList();
    return result;
}

bool PluginHost::runWhilePumping (std::function<void()> fn, int timeoutMs)
{
    return PluginFormats::runWhilePumping (std::move (fn), timeoutMs);
}

//==============================================================================
void PluginHost::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // message thread: the list changed (a scan, a removal, the blacklist)
    saveList();
    registerIntoAttached();
}

void PluginHost::registerIntoAttached()
{
    attached.erase (std::remove_if (attached.begin(), attached.end(),
                                    [] (const Attached& entry) { return entry.registry.expired(); }),
                    attached.end());
    for (const auto& entry : attached)
        if (auto registry = entry.registry.lock())
            registerInto (*registry, entry.mode);
}

} // namespace audium
