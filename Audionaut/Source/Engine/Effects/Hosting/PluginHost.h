//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <JuceHeader.h>

#include "Engine/Effects/Effect.h"
#include "Engine/Effects/EffectRegistry.h"

namespace audium
{

/**
 * The plugins this machine has, and how an engine gets them as effects.
 *
 * One per process (the app, the CLI binary), handed to the factory: it
 * owns the formats this platform hosts - Audio Units on macOS, where the
 * app lives in its sandbox; VST3 on Windows; VST3 and LV2 on Linux - and the
 * list of known plugins, kept as XML in the app's data folder together with
 * the blacklist, so a launch has the plugins without a scan. A scan goes
 * through a child process per file (ChildProcessScanner) when the host is
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
 * Plugins that cannot be created synchronously (AUv3) are listed but not
 * registered yet; they wait for asynchronous loading.
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

    /// The formats this platform hosts, added to the manager.
    static void addPlatformFormats (juce::AudioPluginFormatManager& manager);

    juce::AudioPluginFormatManager& getFormats() noexcept { return formats; }
    juce::KnownPluginList& getList() noexcept { return list; }
    juce::File getListFile() const;
    juce::File getDeadMansPedalFile() const;

    /// Reads the list file (and what the dead man's pedal left behind); false when there is none yet.
    bool loadList();
    /// Writes the list file; false when the folder cannot be written.
    bool saveList() const;

    /// Registers the plugins in the registry now and after every change of the list, while the registry lives.
    void attach (std::shared_ptr<EffectRegistry> registry, Mode mode);
    /// Registers the plugins (one type each), drops types of plugins no longer listed, installs the resolver.
    void registerInto (EffectRegistry& registry, Mode mode) const;

    /// The listed plugin a type id of PluginTypeId::make names.
    std::optional<juce::PluginDescription> findByTypeId (const std::string& typeId) const;
    /// A listed plugin of that name and manufacturer this host can create, any format.
    std::optional<juce::PluginDescription> findSubstitute (const juce::String& name, const juce::String& manufacturer) const;
    /// Whether this host creates the plugin (its format is here, and it loads synchronously).
    bool canCreate (const juce::PluginDescription& description) const;
    /// Whether a type id names a plugin of one of this host's formats (PluginTypeId::make of one).
    bool isPluginTypeId (const std::string& typeId) const;

    /// Message thread: an instance, or nullptr with the reason in `error`.
    std::unique_ptr<juce::AudioPluginInstance> createInstance (const juce::PluginDescription& description,
                                                               double sampleRate, int blockSize, juce::String& error) const;
    /// Message thread: the plugin as an effect; a placeholder when it fails to load or in catalogue-only mode.
    std::unique_ptr<Effect> createEffect (const juce::PluginDescription& description, Mode mode) const;

    /// Scans with a child process per file: this executable run as the scan-plugin worker.
    void useChildProcessScanner (juce::File executable, int timeoutMs = 60000);

    struct ScanResult
    {
        int found = 0;         ///< plugins in the list after the scan
        int added = 0;         ///< ... of which new
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
    std::unique_ptr<Effect> createEffect (const juce::PluginDescription&, const EffectTypeInfo& type, Mode mode) const;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    struct Attached
    {
        std::weak_ptr<EffectRegistry> registry;
        Mode mode;
    };

    juce::File dataDirectory;
    juce::AudioPluginFormatManager formats;
    juce::KnownPluginList list;
    std::vector<Attached> attached;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginHost)
};

} // namespace audium
