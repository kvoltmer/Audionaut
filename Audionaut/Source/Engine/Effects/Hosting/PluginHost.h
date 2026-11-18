//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <JuceHeader.h>

#include "Engine/Effects/Effect.h"
#include "Engine/Effects/EffectRegistry.h"

namespace audium
{

class HostedEffect;

/**
 * The plugins this machine has, and how an engine gets them as effects.
 *
 * One per process (the app, the CLI binary), handed to the factory: it
 * owns the formats this platform hosts - Audio Units on macOS, where the
 * app lives in its sandbox; VST3 on Windows; VST3 and LV2 on Linux - and the
 * list of known plugins, kept as XML in the app's data folder together with
 * the blacklist, so a launch has the plugins without a scan. A scan goes
 * through a worker process (ChildProcessScanner) when the host is
 * given the executable; without one it scans in-process (tests).
 *
 * attach() registers every known plugin as an effect type in an engine's
 * registry, by manufacturer, and again whenever the list changes; the
 * substitute resolver it installs finds a project's plugin in another
 * format by name and manufacturer. In catalogue-only mode the types are
 * there but creating one gives a placeholder that carries the state without
 * loading the plugin: the scratch engine of a hosted edit needs the types
 * to resolve, not a second instance of every plugin in the project.
 *
 * A plugin that runs in a process of its own (AUv3; on macOS, inside the
 * sandbox, an AUv2 without the sandbox-safe flag, which the OS refuses to
 * open in-process - these go to the system's hosting service, as in Logic)
 * is created asynchronously: its effect starts as a loading placeholder
 * that passes the signal through and adopts the plugin when it arrives.
 * waitForPendingLoads() is for a process that wants them before it goes on
 * (the CLI, before it reports or saves): with a registry it waits for that
 * engine's plugins only - a verb on a scratch engine shares the host with
 * the app and is not kept by a plugin the app's own project is loading.
 *
 * The app hosts plugins only once its user turned them on (setEnabled;
 * a host is on unless told otherwise). Off, no plugin is offered as an
 * insert (Availability::disabled) and a project's slot with one stays an
 * unresolved element with its state.
 *
 * Before macOS loads an AUv2 without the sandbox-safe flag for a sandboxed
 * app, it asks the user to lower the app's security settings. That
 * question is only raised where it was allowed (setLoweredSecurityAllowed):
 * the app does so together with turning the plugins on, a headless run
 * never does - it has nobody to answer. Not allowed, such a plugin is not
 * offered either (Availability::needsLoweredSecurity), and a scan does
 * not load it. Such plugins are loaded one at a time: when the user
 * declines the question, the plugins waiting behind that one are not
 * loaded either, so a project with many of them asks once.
 *
 * Where the system knows its plugins without loading them (the Audio
 * Units on macOS), the list has them without a scan: syncWithInstalled()
 * adds what is installed as not scanned (isScanned). Such a plugin is
 * offered like any other and first loaded when it is used; a scan
 * replaces the entry with what the loaded plugin says. Where such a plugin
 * would run inside this process, its first load is preceded by a check in
 * the scan worker (setChecksBeforeFirstLoad): the effect waits as a
 * loading placeholder meanwhile, and a plugin that takes the worker down
 * is skipped instead of taking the app down.
 *
 * What the user takes off the list (remove, clear) leaves it, unless the
 * system lists the plugin by itself: such an entry would be back with the
 * next look at what is installed, so it stays, marked as removed
 * (isRemoved), and is not offered (Availability::removed) until it is put
 * back (restore) or a scan puts all of them back. The marks are kept in
 * the list file.
 */
class PluginHost final : private juce::ChangeListener
{
public:
    enum class Mode
    {
        full,            ///< effects run the plugin
        catalogueOnly    ///< types registered, effects are placeholders carrying the state
    };

    /// `dataDirectory`: where the list lives (a Plugins folder is made in it on demand). Without the
    /// platform's formats the host has none until given some (tests bring their own).
    explicit PluginHost (juce::File dataDirectory, bool withPlatformFormats = true);
    ~PluginHost() override;

    /// The app's data folder, the default place for the list.
    static juce::File defaultDataDirectory();

    /// The formats this platform hosts, added to the manager. Plugins that need the user's consent to
    /// lowered security settings are not loaded until applyLoweredSecurity() says so.
    static void addPlatformFormats (juce::AudioPluginFormatManager& manager);
    /// Whether the manager's formats load the plugins the system asks the user about (an AUv2 without
    /// the sandbox-safe flag, in a sandboxed process: out of process, after the user's consent).
    static void applyLoweredSecurity (juce::AudioPluginFormatManager& manager, bool allowed);
    /// Whether this process is one in which that question comes up (a sandboxed app on macOS).
    static bool isSandboxed();

    /// Message thread. Whether this host offers and loads plugins at all (it does unless told otherwise;
    /// the app turns it off until its user turned plugins on). Registers the attached registries' types
    /// again when it changes. What an engine already has stays as it is: a plugin that runs keeps
    /// running, a slot that was left unresolved stays so until its project is read again.
    void setEnabled (bool enabled);
    bool isEnabled() const noexcept { return enabled; }

    /// Message thread. Whether plugins are loaded that make the system ask the user to lower the app's
    /// security settings. Off (the default), they are not available here and the question is never
    /// raised. Registers the attached registries' types again when it changes.
    void setLoweredSecurityAllowed (bool allowed);
    bool isLoweredSecurityAllowed() const noexcept { return loweredSecurityAllowed->load(); }
    /// Both at once, as the app's switch sets them: the types are registered again once, not twice.
    void setEnabled (bool enabled, bool loweredSecurityAllowed);
    /// Which plugins those are; the platform's answer by default (tests bring their own).
    void setLoweredSecurityQuestion (std::function<bool (const juce::PluginDescription&)> asksForIt) { asksForLoweredSecurity = std::move (asksForIt); }

    /// What is installed, as far as the system tells without loading anything: the platform's answer
    /// by default (the Audio Units on macOS, nothing elsewhere; tests bring their own).
    void setInstalledSource (std::function<std::vector<juce::PluginDescription>()> source) { installedSource = std::move (source); }
    /// Message thread. Brings the list in line with what is installed: adds the plugins that are
    /// neither listed nor skipped as not scanned, and drops not-scanned entries that are gone.
    /// True when the list changed.
    bool syncWithInstalled();
    /// Whether the entry comes from a scan (the loaded plugin's word) or only from the system's list.
    static bool isScanned (const juce::PluginDescription& description) noexcept { return description.lastInfoUpdateTime != juce::Time(); }
    /// Message thread, after a scan of the list's (which loads the not-scanned entries too and
    /// replaces them where they are): a not-scanned plugin the scan loaded and found nothing in is
    /// skipped from now on, the list is in line with what is installed, and its listeners hear of it.
    void finishScan();
    /// The scanner's word that a worker loaded the file and found no plugin in it. Any thread.
    void noteLoadedEmpty (const juce::String& fileOrIdentifier);
    /// Whether a not-scanned plugin that runs inside this process is scanned (by the list's scanner,
    /// off the message thread) before it is first loaded. Off by default: only a scanner in another
    /// process makes that a safeguard, and the effect arrives later for it (an app turns it on, a
    /// command that waits for its plugins does not).
    void setChecksBeforeFirstLoad (bool checks_) noexcept { checksBeforeFirstLoad = checks_; }

    /// Message thread. Takes the plugins off the list. One the system lists by itself stays on it,
    /// marked as removed and not offered any more; the others are gone until a scan finds them again.
    void remove (const std::vector<juce::PluginDescription>& descriptions);
    /// Whether the entry is one the user removed and the system still lists.
    bool isRemoved (const juce::PluginDescription& description) const { return removed.count (description.fileOrIdentifier) > 0; }
    /// Message thread. Puts a removed plugin back, as it was listed before.
    void restore (const juce::PluginDescription& description);
    /// Message thread. Puts every removed plugin back (one format's with `onlyFormat`), as a scan does first.
    void restoreRemoved (const juce::String& onlyFormat = {});
    /// Message thread. Forgets what the scans found, the skipped files included. What the system lists
    /// by itself is on the list again, not scanned and removed; elsewhere the list is empty.
    void clear();

    juce::AudioPluginFormatManager& getFormats() noexcept { return formats; }
    juce::KnownPluginList& getList() noexcept { return list; }
    juce::File getListFile() const;
    juce::File getDeadMansPedalFile() const;
    /// What the child-process scanner did, file by file (useChildProcessScanner writes it); a text
    /// file a user can send along. Kept over several scans until it grows past a megabyte.
    juce::File getScanLogFile() const;

    /// Folders a scan looks in besides the formats' standard ones, for the formats that look in folders
    /// at all (VST3, LV2; Audio Units are the system's to find). Kept beside the list, one per line, so
    /// the app and the command line scan the same places.
    juce::FileSearchPath getExtraScanFolders() const;
    /// False when the folder cannot be written.
    bool setExtraScanFolders (const juce::FileSearchPath& folders) const;
    /// Where a scan of the format looks: its standard folders and, if it has any, the extra ones.
    juce::FileSearchPath getScanFolders (juce::AudioPluginFormat& format) const;
    /// Whether one of this host's formats looks in folders, so that extra ones mean something here.
    bool scansFolders() const;

    /// Reads the list file (and what the dead man's pedal left behind); false when there is none yet.
    bool loadList();
    /// Writes the list file; false when the folder cannot be written.
    bool saveList() const;
    /// Writes the list file if the list differs from what was last read or written here. A process that
    /// changed nothing leaves the file alone, so it does not undo what another process (a scan run
    /// beside the app) wrote meanwhile.
    bool saveListIfChanged() const;

    /// Registers the plugins in the registry now and after every change of the list, while the registry lives.
    void attach (std::shared_ptr<EffectRegistry> registry, Mode mode);
    /// Registers the plugins (one type each), drops types of plugins no longer listed, installs the resolver.
    void registerInto (EffectRegistry& registry, Mode mode) const;

    /// The listed plugin a type id of PluginTypeId::make names.
    std::optional<juce::PluginDescription> findByTypeId (const std::string& typeId) const;
    /// A listed plugin of that name and manufacturer this host can create, any format.
    std::optional<juce::PluginDescription> findSubstitute (const juce::String& name, const juce::String& manufacturer) const;
    /// Why a listed plugin is not offered as an insert, if it is not.
    enum class Availability
    {
        available,
        formatMissing,          ///< a format this process does not host
        disabled,               ///< plugins are turned off here
        removed,                ///< the user took it off the list
        needsLoweredSecurity    ///< the system would ask the user to lower the security settings, and that is not allowed here
    };
    Availability availabilityOf (const juce::PluginDescription& description) const;
    /// The reason in words, "" for an available plugin.
    static juce::String describe (Availability availability);
    /// What a plugin's failure to load is called for the user: the format's message, or plainer words
    /// where the system refused (the user declined to lower the security settings).
    static juce::String describeLoadError (const juce::String& message);

    /// Whether this host creates the plugin (its format is here, plugins are on, and nothing has to be allowed first).
    bool canCreate (const juce::PluginDescription& description) const;
    /// Whether the plugin runs in a process of its own (an AUv3; inside the sandbox an AUv2 without the sandbox-safe
    /// flag): its effect is created asynchronously and starts as a loading placeholder.
    bool loadsOutOfProcess (const juce::PluginDescription& description) const;
    /// Whether a type id names a plugin of one of this host's formats (PluginTypeId::make of one).
    bool isPluginTypeId (const std::string& typeId) const;

    /// Message thread: an instance, or nullptr with the reason in `error`. Synchronous: not for a plugin that
    /// loadsOutOfProcess() - createEffect() handles those.
    std::unique_ptr<juce::AudioPluginInstance> createInstance (const juce::PluginDescription& description,
                                                               double sampleRate, int blockSize, juce::String& error) const;
    /// Message thread: the plugin as an effect; a placeholder when it fails to load or in catalogue-only mode,
    /// a loading placeholder for a plugin that comes asynchronously.
    std::unique_ptr<Effect> createEffect (const juce::PluginDescription& description, Mode mode) const;

    /// Message thread: gives a placeholder that has no plugin (a catalogue-only one, or one whose plugin
    /// failed to load) its plugin, with the state it kept. A plugin that loadsOutOfProcess() arrives later
    /// (waitForPendingLoads). False when the plugin cannot be created here or failed to load.
    bool load (HostedEffect& effect) const;

    /// Effects created here whose plugin has not arrived yet, whatever engine they belong to.
    int getNumPendingLoads() const noexcept { return pendingLoads->load(); }
    /// Message thread: pumps messages until every pending plugin arrived; false when the timeout passed
    /// first, or when the message loop was told to stop (nothing arrives any more then).
    bool waitForPendingLoads (int timeoutMs = 30000) const;
    /// The same for the plugins of one registry's effects (one engine's).
    static bool waitForPendingLoads (const EffectRegistry& registry, int timeoutMs = 30000);

    /// Scans in a child process: the executable run as the scan-plugin worker, which serves file after file
    /// until one takes it down (timeoutMs: how long one file may take).
    void useChildProcessScanner (juce::File executable, int timeoutMs = 60000);
    /// The executable an app scans with: the scan helper in its bundle on macOS (a sandboxed app
    /// cannot run itself as a child process), the app itself everywhere else and where there is no helper.
    static juce::File scanWorkerExecutable();

    struct ScanResult
    {
        int found = 0;         ///< scanned plugins in the list after the scan (not those only listed as installed)
        int added = 0;         ///< ... of which scanned by this scan for the first time
        int failed = 0;        ///< files that yielded no plugin (refused to load here: not sandbox-safe, wrong architecture, ...)
        int blacklisted = 0;   ///< ... of which crashed the worker or timed out, and are skipped from now on
    };

    /**
     * Scans every format's default locations (one format's with `onlyFormat`),
     * headless: `progress` hears the fraction done and the file at hand,
     * `shouldAbort` is asked between files. Saves the list when done.
     */
    ScanResult scanAll (const std::function<void (float fraction, const juce::String& file)>& progress = {},
                        const std::function<bool()>& shouldAbort = {},
                        const juce::String& onlyFormat = {});

    /**
     * Message thread: runs `fn` on a thread of its own while this thread
     * pumps messages, for the plugin calls that refuse to run on the message
     * thread and want it free (an AUv3 scan). False when `fn` is not done
     * within the timeout; it then goes on in the background.
     */
    static bool runWhilePumping (std::function<void()> fn, int timeoutMs);

private:
    std::unique_ptr<Effect> createEffect (const juce::PluginDescription&, const EffectTypeInfo& type, Mode mode,
                                          std::shared_ptr<std::atomic<int>> arrivalCount = nullptr) const;
    static bool waitUntilNone (const std::atomic<int>& pending, int timeoutMs);
    juce::AudioPluginFormat* formatOf (const juce::PluginDescription& description) const;   // nullptr: not hosted here
    void requestInstance (HostedEffect& effect) const;   // asynchronously: the effect adopts what comes

    /// The plugins the system asks the user about, waiting to be loaded one after the other.
    struct Questions
    {
        struct Waiting
        {
            std::weak_ptr<const int> alive;   // the effect's
            HostedEffect* effect = nullptr;
            juce::AudioPluginFormat* format = nullptr;
            std::shared_ptr<std::atomic<int>> arrivals;
        };
        std::deque<Waiting> waiting;
        bool busy = false;     // one is loading
        bool closed = false;   // the host is gone, and its formats with it
    };
    static void loadNextAsked (std::shared_ptr<Questions> questions, std::shared_ptr<std::atomic<int>> pending);
    static void settle (const Questions::Waiting& one, std::atomic<int>& pending,
                        std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String& message);

    /// Files a worker loaded and found no plugin in, since the host last asked.
    struct LoadedEmpty
    {
        juce::CriticalSection lock;
        juce::StringArray files;
    };
    bool takeLoadedEmpty (const juce::String& fileOrIdentifier) const;   // whether it was among them; not any more then
    void skipNotScanned (const juce::String& fileOrIdentifier);          // off the list, and skipped from now on
    int numScanned() const;

    /// The checks of not-scanned plugins before their first load, one at a time on a thread of their own.
    struct Checks
    {
        explicit Checks (juce::KnownPluginList& list_) : list (list_) {}
        juce::KnownPluginList& list;
        std::shared_ptr<int> hostAlive = std::make_shared<int> (0);   // expires with the host
        juce::ThreadPool pool { juce::ThreadPoolOptions{}.withNumberOfThreads (1) };
    };
    bool needsCheckBeforeLoad (const juce::PluginDescription& description) const;
    void requestChecked (HostedEffect& effect) const;   // asynchronously: checked, then loaded, the effect adopts what comes
    std::unique_ptr<juce::XmlElement> listAsXml() const;   // the list, its blacklist and the removed marks
    juce::String listAsText() const;
    bool pruneRemoved();   // marks without an entry go; true when one did
    void registerIntoAttached();
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    struct Attached
    {
        std::weak_ptr<EffectRegistry> registry;
        Mode mode;
    };

    std::shared_ptr<std::atomic<int>> pendingLoads = std::make_shared<std::atomic<int>> (0);   // shared with the callbacks in flight

    juce::File dataDirectory;
    juce::AudioPluginFormatManager formats;
    juce::KnownPluginList list;
    std::set<juce::String> removed;   // fileOrIdentifier of the listed entries the user removed
    mutable juce::String persisted;   // the list as this host started with it, or last read from or wrote to the file
    bool enabled = true;
    std::shared_ptr<std::atomic<bool>> loweredSecurityAllowed = std::make_shared<std::atomic<bool>> (false);   // shared with the scanner
    std::function<bool (const juce::PluginDescription&)> asksForLoweredSecurity;
    std::function<std::vector<juce::PluginDescription>()> installedSource;
    std::shared_ptr<Questions> questions = std::make_shared<Questions>();   // shared with the callbacks in flight
    std::unique_ptr<Checks> checks = std::make_unique<Checks> (list);
    bool checksBeforeFirstLoad = false;
    std::shared_ptr<LoadedEmpty> loadedEmpty = std::make_shared<LoadedEmpty>();   // shared with the scanner
    std::vector<Attached> attached;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginHost)
};

} // namespace audium
