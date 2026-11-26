//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#if defined (__APPLE__)
 #include <pwd.h>
 #include <unistd.h>
#endif

#include <JuceHeader.h>

#include "Engine/Effects/Hosting/PluginHost.h"
#include "Engine/Effects/Hosting/PluginListScan.h"
#include "Interface/ColourIds.h"

/**
 * The Manage Plugins window: the plugins this machine's list knows in a
 * table that sorts by its columns, and buttons under it: Rescan looks
 * through the platform's plugin folders (each file in a worker process,
 * so a plugin that crashes takes down only that and is skipped from then
 * on), Clear empties the list, Remove (there while something is selected)
 * takes entries off it. Where a format looks for its plugins in folders
 * (VST3, LV2), a Folders button opens the list of extra ones to scan. Under the list a line counts what the list holds: the
 * effects the inserts can use, the instruments (listed, not hosted) and
 * the files the scan skips.
 *
 * Above the list a tick box turns plugins off and on. Off, no plugin is
 * offered as an insert, none is loaded and the list cannot be scanned;
 * on (the default), all of them are there. The choice goes to
 * `onEnabledChanged`, which stores it and tells the host.
 *
 * Where the platform has several formats to choose from (Linux: LV2 and
 * VST3, see PluginHost::formatsAreSwitchable), a tick box per format
 * stands beside it. A format that is off is not scanned and its plugins
 * are not offered; they stay in the list, greyed out. Turning one on whose
 * plugins the list does not have yet starts a scan. The choice goes to
 * `onFormatEnabledChanged`, which stores it and tells the host.
 *
 * Nothing is scanned here unless Scan is clicked (the app's first launch
 * runs one scan of its own where plugins are found in folders, see
 * InitialPluginScanComponent). Where the system knows its
 * plugins without loading them (the Audio Units on macOS) the host lists
 * them all the same, and the Status column says "Not scanned" for those:
 * they are offered as inserts and first loaded when they are used.
 * Removing such a plugin leaves its row, with the Status "Removed" and not
 * offered, until its menu or a scan puts it back; Clear does that to all.
 *
 * The rows are drawn here (Rows): an entry the inserts cannot use - an
 * instrument, a plugin of a format not hosted, a file the scan skips - is
 * greyed out, and what the selected entry is, with the reason where it is
 * not offered, stands in two lines under the list.
 *
 * A scan (audium::PluginListScan: the formats' folders, file after file on
 * a thread of its own) shows in the same place while it runs: a progress
 * bar, the plugin at hand and a Cancel button; the tick box and the buttons
 * wait until it is done. What the scan left out is said once, at the end.
 *
 * The list is the PluginHost's: a scan here reaches the engine's effect
 * registry through the host, and the insert menus list the plugins at once.
 */
class PluginManagerComponent : public juce::Component,
                               private juce::ChangeListener
{
public:
    explicit PluginManagerComponent (std::shared_ptr<audium::PluginHost> host_,
                                     std::function<void (bool)> onEnabledChanged_ = {},
                                     std::function<void (const juce::String&, bool)> onFormatEnabledChanged_ = {}) :
        host (std::move (host_)),
        onEnabledChanged (std::move (onEnabledChanged_)),
        onFormatEnabledChanged (std::move (onFormatEnabledChanged_))
    {
        using Header = juce::TableHeaderComponent;
        auto& header = table.getHeader();
        header.addColumn (TRANS ("Name"),         Rows::nameColumn,         210, 100, 700);
        header.addColumn (TRANS ("Format"),       Rows::formatColumn,       80, 80, 80, Header::notResizable);
        header.addColumn (TRANS ("Category"),     Rows::categoryColumn,     90, 70, 200);
        header.addColumn (TRANS ("Manufacturer"), Rows::manufacturerColumn, 170, 100, 300, Header::defaultFlags | Header::sortedForwards);   // until a column is clicked
        header.addColumn (TRANS ("Version"),      Rows::versionColumn,      70, 60, 300, Header::notSortable);
        header.addColumn (TRANS ("Status"),       Rows::statusColumn,       90, 80, 200, Header::notSortable);
        header.setStretchToFitActive (true);   // the columns share the table's width, whatever size the window was left at: no horizontal scroll bar
        table.setHeaderHeight (22);
        table.setRowHeight (20);
        table.setModel (&rows);
        table.setMultipleSelectionEnabled (true);
        addAndMakeVisible (table);

        rescan.setButtonText (TRANS ("Scan"));
        rescan.setTooltip (TRANS ("Loads every plugin once, in a process of its own, to see that it works: looks through the plugin folders for new and updated plugins, "
                                  "and checks the ones listed as not scanned."));
        rescan.onClick = [this] { scanAllFormats(); };
        addAndMakeVisible (rescan);
        clear.setButtonText (TRANS ("Clear"));
        clear.setTooltip (TRANS ("Forgets what the scans found, the skipped files included, and takes every plugin off the insert menus; Scan brings them back."));
        clear.onClick = [this] { host->clear(); };
        addAndMakeVisible (clear);
        remove.setButtonText (TRANS ("Remove"));
        remove.setTooltip (TRANS ("Takes the selected entries off the list; a skipped file is tried again by the next scan. "
                                  "A plugin the system lists by itself stays as removed and is not offered until it is put back."));
        remove.onClick = [this] { removeSelected(); };
        addChildComponent (remove);   // shown while something is selected
        scanFolders.setButtonText (TRANS ("Folders..."));
        scanFolders.setTooltip (TRANS ("Folders to look for plugins in, besides the standard ones."));
        scanFolders.onClick = [this] { editFolders(); };
        addChildComponent (scanFolders);
        scanFolders.setVisible (host->scansFolders());   // not where the system finds the plugins (Audio Units)

        // a scan's progress, in the place of the details while it runs
        scanBar.setPercentageDisplay (false);
        addChildComponent (scanBar);
        scanFile.setFont (juce::FontOptions (12.0f));
        scanFile.setMinimumHorizontalScale (1.0f);
        addChildComponent (scanFile);
        cancelScan.setButtonText (TRANS ("Cancel"));
        cancelScan.onClick = [this]
        {
            if (scan != nullptr)
                scan->cancel();   // it gives up the file at hand and the formats still waiting, and says so as any scan does
        };
        addChildComponent (cancelScan);

        details.setJustificationType (juce::Justification::topLeft);
        details.setFont (juce::FontOptions (12.0f));
        details.setMinimumHorizontalScale (1.0f);
        addAndMakeVisible (details);

        status.setJustificationType (juce::Justification::centredLeft);
        status.setFont (juce::FontOptions (12.0f));
        status.setColour (juce::Label::textColourId, findColour (audium::defaultTextColourId).withAlpha (0.75f));
        addAndMakeVisible (status);

        enablePlugins.setButtonText (TRANS ("Plugins enabled"));
        enablePlugins.setTooltip (TRANS ("Off, Audionaut offers no plugins and opens a project without them; a project that uses one keeps the slot and its settings. "
                                         "A project that is open follows when it is opened again."));
        enablePlugins.setToggleState (host->isEnabled(), juce::dontSendNotification);
        enablePlugins.onClick = [this] { applyEnabled (enablePlugins.getToggleState()); };
        addAndMakeVisible (enablePlugins);

        // a switch per format, where there are several to choose from (Linux)
        if (audium::PluginHost::formatsAreSwitchable() && host->getFormats().getNumFormats() > 1)
        {
            for (auto* format : host->getFormats().getFormats())
            {
                const auto name = format->getName();
                auto* formatSwitch = formatSwitches.add (new juce::ToggleButton (name + TRANS (" plugins")));
                formatSwitch->setTooltip (TRANS ("Off, Audionaut neither looks for plugins of this format nor offers them; a project that uses one gets the same plugin "
                                                 "in a format that is on, where it is installed. Turning a format on scans for its plugins."));
                formatSwitch->setToggleState (host->isFormatEnabled (name), juce::dontSendNotification);
                formatSwitch->onClick = [this, name, formatSwitch] { applyFormatEnabled (name, formatSwitch->getToggleState()); };
                addAndMakeVisible (formatSwitch);
            }
        }

        showLog.setButtonText (TRANS ("Show Scan Log"));
        showLog.setTooltip (TRANS ("Shows the file that says what the last scans did with each plugin: the one to send along when a scan goes wrong."));
        showLog.onClick = [this] { host->getScanLogFile().revealToUser(); };
        addAndMakeVisible (showLog);

        host->getList().addChangeListener (this);
        listChanged();
        pluginsSwitched();
    }

    ~PluginManagerComponent() override
    {
        scan.reset();   // ends a scan that still runs, before anything it reports to goes
        host->getList().removeChangeListener (this);
        table.setModel (nullptr);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (findColour (audium::secondaryBackgroundColourId));
    }

    void resized() override
    {
        const auto buttonHeight = 20;   // as the keys in the main window's header
        auto bounds = getLocalBounds().reduced (8);
        auto top = bounds.removeFromTop (24);
        enablePlugins.setBounds (top.removeFromLeft (160));
        for (auto* formatSwitch : formatSwitches)
            formatSwitch->setBounds (top.removeFromLeft (120));
        bounds.removeFromTop (4);
        auto bottom = bounds.removeFromBottom (22);
        showLog.setBounds (bottom.removeFromRight (110).withSizeKeepingCentre (110, buttonHeight));
        bottom.removeFromRight (8);
        status.setBounds (bottom);
        auto below = bounds.removeFromBottom (38);
        details.setBounds (below);
        auto progress = below.withSizeKeepingCentre (below.getWidth(), 22);
        cancelScan.setBounds (progress.removeFromRight (80).withSizeKeepingCentre (80, buttonHeight));
        progress.removeFromRight (8);
        scanBar.setBounds (progress.removeFromLeft (juce::jmin (260, progress.getWidth() / 2)).withSizeKeepingCentre (juce::jmin (260, progress.getWidth() / 2), 20));
        progress.removeFromLeft (8);
        scanFile.setBounds (progress);
        bounds.removeFromBottom (4);
        auto buttons = bounds.removeFromBottom (buttonHeight);
        scanFolders.setBounds (buttons.removeFromRight (80));
        for (auto* button : { &rescan, &clear, &remove })
        {
            button->setBounds (buttons.removeFromLeft (80));
            buttons.removeFromLeft (8);
        }
        bounds.removeFromBottom (9);
        table.setBounds (bounds);
    }

private:
    /// The list's rows: the plugins, then the files the scan skips, the entries the inserts cannot
    /// use greyed out. Sorts the host's list by the column clicked and tells the window the selection.
    class Rows final : public juce::TableListBoxModel
    {
    public:
        explicit Rows (PluginManagerComponent& owner_) : owner (owner_) {}

        enum { nameColumn = 1, formatColumn, categoryColumn, manufacturerColumn, versionColumn, statusColumn };

        int getNumRows() override
        {
            return static_cast<int> (owner.listed.size()) + owner.skippedFiles.size();
        }

        void paintRowBackground (juce::Graphics& g, int, int, int, bool rowIsSelected) override
        {
            const auto background = owner.table.findColour (juce::ListBox::backgroundColourId);
            g.fillAll (rowIsSelected ? background.interpolatedWith (owner.table.findColour (juce::ListBox::textColourId), 0.5f)
                                     : background);
        }

        void paintCell (juce::Graphics& g, int row, int columnId, int width, int height, bool) override
        {
            juce::String text;
            auto usable = false;
            if (const auto skipped = owner.skippedFileAt (row); skipped.isNotEmpty())
            {
                const auto& entry = owner.skippedEntry (skipped);
                if (columnId == nameColumn)
                    text = entry.name;
                else if (columnId == formatColumn)
                    text = entry.format;
                else if (columnId == statusColumn)
                    text = TRANS ("Skipped");
            }
            else if (const auto description = owner.pluginAt (row))
            {
                usable = owner.isOfferedAt (row);
                switch (columnId)
                {
                    case nameColumn:         text = description->name; break;
                    case formatColumn:       text = description->pluginFormatName; break;
                    case categoryColumn:     text = description->category.isNotEmpty() ? description->category : "-"; break;
                    case manufacturerColumn: text = description->manufacturerName; break;
                    case versionColumn:      text = description->version; break;
                    case statusColumn:       text = owner.host->isRemoved (*description) ? TRANS ("Removed")
                                                  : audium::PluginHost::isScanned (*description) ? juce::String() : TRANS ("Not scanned"); break;
                    default: break;
                }
            }

            if (text.isEmpty())
                return;

            const auto colour = owner.table.findColour (juce::ListBox::textColourId);
            g.setColour (! usable ? colour.withMultipliedAlpha (0.4f)
                                  : columnId == nameColumn ? colour : colour.withMultipliedAlpha (0.7f));
            g.setFont (owner.table.withDefaultMetrics (juce::FontOptions ((float) height * 0.7f, juce::Font::bold)));
            g.drawFittedText (text, 4, 0, width - 6, height, juce::Justification::centredLeft, 1, 0.9f);
        }

        void cellClicked (int row, int columnId, const juce::MouseEvent& e) override
        {
            juce::TableListBoxModel::cellClicked (row, columnId, e);
            if (row >= 0 && row < getNumRows() && e.mods.isPopupMenu())
                owner.rowMenu (row).showMenuAsync (juce::PopupMenu::Options().withTargetComponent (e.originalComponent)
                                                                             .withMousePosition()
                                                                             .withDeletionCheck (owner));
        }

        void selectedRowsChanged (int) override
        {
            owner.scanOutcome.clear();
            owner.selectionChanged();
        }
        void deleteKeyPressed (int) override      { owner.removeSelected(); }

        void sortOrderChanged (int columnId, bool isForwards) override
        {
            switch (columnId)
            {
                case nameColumn:         plugins().sort (juce::KnownPluginList::sortAlphabetically, isForwards); break;
                case formatColumn:       plugins().sort (juce::KnownPluginList::sortByFormat, isForwards); break;
                case categoryColumn:     plugins().sort (juce::KnownPluginList::sortByCategory, isForwards); break;
                case manufacturerColumn: plugins().sort (juce::KnownPluginList::sortByManufacturer, isForwards); break;
                default: break;
            }
        }

        static juce::String versionAndName (const juce::PluginDescription& description)
        {
            juce::StringArray items;
            if (description.descriptiveName != description.name)
                items.add (description.descriptiveName);
            items.add (description.version);
            items.removeEmptyStrings();
            return items.joinIntoString (" - ");
        }

    private:
        juce::KnownPluginList& plugins() const   { return owner.host->getList(); }

        PluginManagerComponent& owner;
    };

    /// The extra folders in a list to add to and take from; what it holds when its window closes is
    /// what the next scan looks in.
    struct FoldersEditor final : public juce::Component
    {
        explicit FoldersEditor (std::shared_ptr<audium::PluginHost> host_) :
            host (std::move (host_))
        {
            hint.setText (TRANS ("Audionaut looks for plugins in the standard folders of each format. Folders listed here are scanned as well."),
                          juce::dontSendNotification);
            hint.setFont (juce::FontOptions (12.0f));
            hint.setJustificationType (juce::Justification::topLeft);
            addAndMakeVisible (hint);
            list.setPath (host->getExtraScanFolders());
            addAndMakeVisible (list);
            setSize (480, 280);
        }

        ~FoldersEditor() override
        {
            host->setExtraScanFolders (list.getPath());
        }

        void resized() override
        {
            auto bounds = getLocalBounds().reduced (8);
            hint.setBounds (bounds.removeFromTop (34));
            list.setBounds (bounds);
        }

        std::shared_ptr<audium::PluginHost> host;
        juce::Label hint;
        juce::FileSearchPathListComponent list;
    };

    void editFolders()
    {
        juce::DialogWindow::LaunchOptions options;
        options.content.setOwned (new FoldersEditor (host));
        options.dialogTitle = TRANS ("Plugin Folders");
        options.dialogBackgroundColour = findColour (audium::secondaryBackgroundColourId);
        options.componentToCentreAround = this;
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar = true;
        options.resizable = true;
        options.launchAsync();
    }

    juce::PopupMenu rowMenu (int row)
    {
        // the entry itself, not its row: the list may be sorted anew before the menu is answered
        const auto* description = pluginAt (row);
        const auto plugin = description != nullptr ? std::optional (*description) : std::nullopt;
        const auto skipped = skippedFileAt (row);

        juce::PopupMenu menu;
        if (plugin && host->isRemoved (*plugin))
            menu.addItem (juce::PopupMenu::Item (TRANS ("Put plug-in back on the list"))
                              .setEnabled (! isScanning())
                              .setAction ([this, plugin] { if (! isScanning()) host->restore (*plugin); }));
        else
            menu.addItem (juce::PopupMenu::Item (TRANS ("Remove plug-in from list"))
                              .setEnabled (! isScanning() && isRemovableAt (row))
                              .setAction ([this, plugin, skipped] { removeEntry (plugin, skipped); }));
        if (const auto file = fileOfRow (row); file.exists())   // not for a plugin whose file is not known (the system's own Audio Units)
            menu.addItem (juce::PopupMenu::Item (TRANS ("Show folder containing plug-in"))
                              .setAction ([file] { file.revealToUser(); }));
        return menu;
    }

    /// The file a row stands for; none where it cannot be told (an Audio Unit that is no component in
    /// the plugin folders: the system's own, or one that comes with an app).
    juce::File fileOfRow (int row) const
    {
        const auto description = pluginAt (row);
        const auto fileOrIdentifier = description ? description->fileOrIdentifier : skippedFileAt (row);
        if (juce::File::isAbsolutePath (fileOrIdentifier))
            return juce::File (fileOrIdentifier);
        return audioUnitComponent (fileOrIdentifier);
    }

    /// The component bundle of an Audio Unit, which the list knows by its codes only
    /// ("AudioUnit:Effects/aufx,subt,manu"): the one in the plugin folders whose Info.plist names them.
    /// The folders are read once, when first asked, and again after the list changed.
    juce::File audioUnitComponent (const juce::String& identifier) const
    {
        if (! identifier.startsWith ("AudioUnit:"))
            return {};

        if (! components)
            components = readAudioUnitComponents();
        const auto found = components->find (identifier.fromLastOccurrenceOf ("/", false, false));
        return found != components->end() ? found->second : juce::File();
    }

    /// "type,subtype,manufacturer" to the bundle that declares it, for the components in the system's
    /// and the user's plugin folder. A bundle whose Info.plist is not XML is not among them.
    static std::map<juce::String, juce::File> readAudioUnitComponents()
    {
        std::map<juce::String, juce::File> result;
       #if JUCE_MAC
        const std::function<void (const juce::XmlElement&, const juce::File&)> collect = [&] (const juce::XmlElement& element, const juce::File& bundle)
        {
            if (element.hasTagName ("dict"))
            {
                juce::StringPairArray entries;
                for (auto* key = element.getChildByName ("key"); key != nullptr; key = key->getNextElementWithTagName ("key"))
                    if (auto* value = key->getNextElement(); value != nullptr && value->hasTagName ("string"))
                        entries.set (key->getAllSubText(), value->getAllSubText());
                if (entries["type"].isNotEmpty() && entries["subtype"].isNotEmpty() && entries["manufacturer"].isNotEmpty())
                    result.emplace (entries["type"] + "," + entries["subtype"] + "," + entries["manufacturer"], bundle);
            }
            for (auto* child : element.getChildIterator())
                collect (*child, bundle);
        };

        // the user's real home: inside the sandbox the home the app is told of is its container
        juce::Array<juce::File> folders { juce::File ("/Library/Audio/Plug-Ins/Components") };
        if (const auto* user = getpwuid (getuid()); user != nullptr && user->pw_dir != nullptr)
            folders.add (juce::File (juce::String::fromUTF8 (user->pw_dir)).getChildFile ("Library/Audio/Plug-Ins/Components"));

        for (const auto& folder : folders)
            for (const auto& bundle : folder.findChildFiles (juce::File::findFilesAndDirectories, false, "*.component"))
                if (const auto plist = juce::parseXML (bundle.getChildFile ("Contents/Info.plist")))
                    collect (*plist, bundle);
       #endif
        return result;
    }

    bool isScanning() const   { return scanBar.isVisible(); }

    /// Takes a plugin, or a skipped file, off the list; not while a scan fills it.
    void removeEntry (const std::optional<juce::PluginDescription>& plugin, const juce::String& skipped)
    {
        if (isScanning())
            return;

        if (plugin)
        {
            host->remove ({ *plugin });   // a plugin the system lists by itself stays, as removed
        }
        else if (skipped.isNotEmpty())
        {
            host->getList().removeFromBlacklist (skipped);   // the next scan tries it again
            host->syncWithInstalled();                       // one the system lists by itself is back, not scanned
        }
    }

    /// Whether taking the row off the list changes anything: not for a plugin that is removed already.
    bool isRemovableAt (int row) const
    {
        if (const auto* description = pluginAt (row))
            return ! host->isRemoved (*description);
        return skippedFileAt (row).isNotEmpty();
    }

    bool isAnySelectedRemovable() const
    {
        const auto selected = table.getSelectedRows();
        for (auto i = 0; i < selected.size(); ++i)
            if (isRemovableAt (selected[i]))
                return true;
        return false;
    }

    void removeSelected()
    {
        if (isScanning())
            return;

        // by what the rows are now: the list tells of each removal later, all at once
        const auto selected = table.getSelectedRows();
        std::vector<juce::PluginDescription> plugins;
        auto anySkipped = false;
        for (auto row = rows.getNumRows(); --row >= 0;)
        {
            if (! selected.contains (row) || ! isRemovableAt (row))
                continue;
            if (const auto* description = pluginAt (row))
            {
                plugins.push_back (*description);
            }
            else if (const auto skipped = skippedFileAt (row); skipped.isNotEmpty())
            {
                host->getList().removeFromBlacklist (skipped);
                anySkipped = true;
            }
        }
        if (! plugins.empty())
            host->remove (plugins);   // a plugin the system lists by itself stays, as removed
        if (anySkipped)
            host->syncWithInstalled();   // one the system lists by itself is back, not scanned
    }

    /// The rows as the table shows them: the host's list and what it skips, copied when either changed,
    /// with whether the inserts have each plugin - painting a cell asks neither the list nor the system.
    void refreshRows()
    {
        listed.clear();
        for (const auto& description : host->getList().getTypes())
            listed.push_back ({ description, isOffered (description) });
        skippedFiles = host->getList().getBlacklistedFiles();
        components.reset();   // a scan may have found what was installed meanwhile
    }

    const juce::PluginDescription* pluginAt (int row) const
    {
        return row >= 0 && row < static_cast<int> (listed.size()) ? &listed[static_cast<size_t> (row)].description : nullptr;
    }

    bool isOfferedAt (int row) const
    {
        return row >= 0 && row < static_cast<int> (listed.size()) && listed[static_cast<size_t> (row)].offered;
    }

    /// The skipped file a row stands for, "" where the row is a plugin (or none).
    juce::String skippedFileAt (int row) const
    {
        return skippedFiles[row - static_cast<int> (listed.size())];
    }

    struct SkippedEntry
    {
        juce::String name, format;
    };

    /// What a skipped file is called: the plugin's name as its format knows it without loading it,
    /// else the file's name. Asked of the formats once per file.
    const SkippedEntry& skippedEntry (const juce::String& fileOrIdentifier)
    {
        if (const auto known = skippedEntries.find (fileOrIdentifier); known != skippedEntries.end())
            return known->second;

        SkippedEntry entry { juce::File::isAbsolutePath (fileOrIdentifier) ? juce::File (fileOrIdentifier).getFileName() : fileOrIdentifier, {} };
        for (auto* format : host->getFormats().getFormats())
        {
            if (format->fileMightContainThisPluginType (fileOrIdentifier))
            {
                if (const auto name = format->getNameOfPluginFromIdentifier (fileOrIdentifier); name.isNotEmpty() && name != fileOrIdentifier)
                    entry.name = name;
                entry.format = format->getName();
                break;
            }
        }
        return skippedEntries.emplace (fileOrIdentifier, std::move (entry)).first->second;
    }

    bool isOffered (const juce::PluginDescription& description) const
    {
        return ! description.isInstrument && host->canCreate (description);
    }

    /// What the selected entry is, and why the inserts do not have it where they do not.
    void updateDetails()
    {
        if (scanOutcome.isNotEmpty())   // the last scan's word stays until another entry is selected
        {
            details.setColour (juce::Label::textColourId, findColour (audium::defaultTextColourId).withAlpha (0.9f));
            details.setText (scanOutcome, juce::dontSendNotification);
            return;
        }

        juce::String text;
        auto usable = true;

        if (table.getNumSelectedRows() > 1)
        {
            text << table.getNumSelectedRows() << TRANS (" entries selected");
        }
        else if (const auto row = table.getSelectedRow(); row >= 0)
        {
            if (const auto skipped = skippedFileAt (row); skipped.isNotEmpty())
            {
                usable = false;
                const auto& entry = skippedEntry (skipped);
                text << entry.name << (entry.name != skipped ? " (" + skipped + ")" : juce::String()) << "\n"
                     << TRANS ("Skipped: it crashed, took too long or did not load when it was scanned. Remove it from the list and the next scan tries it again.");
            }
            else if (const auto description = pluginAt (row))
            {
                juce::StringArray what;
                what.add (description->name);
                what.add (Rows::versionAndName (*description));
                what.add (description->manufacturerName);
                what.add (description->pluginFormatName);
                what.add (description->category);
                what.removeEmptyStrings();
                text << what.joinIntoString (", ") << "\n";

                usable = isOfferedAt (row);
                if (description->isInstrument)
                    text << TRANS ("An instrument: listed, but only effects are used as inserts.");
                else if (host->isRemoved (*description))
                    text << TRANS ("Removed: not offered as an insert. Its menu puts it back on the list, and so does Scan.");
                else if (! usable)
                    text << TRANS ("Not offered as an insert: ") << audium::PluginHost::describe (host->availabilityOf (*description)) << ".";
                else if (! audium::PluginHost::isScanned (*description))
                    text << TRANS ("Offered as an insert. Not scanned: listed as the system reports it, and first loaded when it is used.");
                else
                    text << TRANS ("Offered as an insert.");

                if (juce::File::isAbsolutePath (description->fileOrIdentifier))
                    text << " " << description->fileOrIdentifier;
            }
        }

        details.setColour (juce::Label::textColourId, findColour (audium::defaultTextColourId).withAlpha (usable ? 0.9f : 0.6f));
        details.setText (text, juce::dontSendNotification);
    }

    /// Up to `limit` of the files' names, one per line, the rest counted.
    juce::String namesOf (const juce::StringArray& files, int limit = 8)
    {
        juce::StringArray names;
        for (const auto& file : files)
            names.addIfNotAlreadyThere (skippedEntry (file).name);
        names.sortNatural();

        const auto more = names.size() - limit;
        if (more > 1)   // a last line "and 1 more" would as well be the name
            names.removeRange (limit, more);

        auto text = names.joinIntoString ("\n");
        if (more > 1)
            text << "\n" << TRANS ("and ") << more << TRANS (" more");
        return text;
    }

    /// Says what a scan left out, where it left something out: the names only, the rest is in the list.
    void scanFinished (const audium::PluginListScan::Outcome& outcome)
    {
        scan.reset();
        showScanProgress (false);

        // a scan with nothing to do is over at once: say that it ran
        const auto added = outcome.added;
        scanOutcome = (outcome.cancelled ? TRANS ("Scan cancelled: ") : TRANS ("Scan complete: "))
                      + (added == 0 ? TRANS ("no new plugins found.")
                                    : added == 1 ? TRANS ("1 plugin scanned.")
                                                 : juce::String (added) + TRANS (" plugins scanned."));
        updateDetails();
        updateStatus();   // the log is there now, whether or not the list changed

        const auto& failed = outcome.failed;
        const auto& skipped = outcome.skipped;

        juce::StringArray parts;
        if (! skipped.isEmpty())
            parts.add ((skipped.size() == 1 ? TRANS ("1 plugin crashed or took too long and is skipped:")
                                            : juce::String (skipped.size()) + TRANS (" plugins crashed or took too long and are skipped:"))
                       + "\n" + namesOf (skipped));
        if (! failed.isEmpty())
            parts.add ((failed.size() == 1 ? TRANS ("1 file could not be loaded as a plugin:")
                                           : juce::String (failed.size()) + TRANS (" files could not be loaded as plugins:"))
                       + "\n" + namesOf (failed));
        if (parts.isEmpty())
            return;

        if (! skipped.isEmpty())
            parts.add (TRANS ("Skipped plugins are greyed out in the list."));

        scanResult = juce::NativeMessageBox::showScopedAsync (juce::MessageBoxOptions::makeOptionsOk (juce::MessageBoxIconType::InfoIcon,
                                                                                                      TRANS ("Plugin scan complete"),
                                                                                                      parts.joinIntoString ("\n\n"),
                                                                                                      {},
                                                                                                      this),
                                                              nullptr);
    }

    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        listChanged();
    }

    void listChanged()
    {
        table.getHeader().reSortTable();   // new entries take their place
        refreshRows();
        table.updateContent();
        table.resized();   // the columns share what a scroll bar that came or went leaves them
        table.repaint();
        updateStatus();
        selectionChanged();   // the row may be another entry now
    }

    void applyEnabled (bool enabled)
    {
        if (onEnabledChanged)
            onEnabledChanged (enabled);
        else
            host->setEnabled (enabled);
        pluginsSwitched();
    }

    void applyFormatEnabled (const juce::String& formatName, bool enabled)
    {
        if (onFormatEnabledChanged)
            onFormatEnabledChanged (formatName, enabled);
        else
            host->setFormatEnabled (formatName, enabled);
        pluginsSwitched();

        // a format turned on for the first time has nothing in the list yet: looked for at once
        if (enabled && host->isEnabled() && ! isScanning() && ! listsFormat (formatName))
            scanAllFormats();
    }

    bool listsFormat (const juce::String& formatName) const
    {
        for (const auto& description : host->getList().getTypes())
            if (description.pluginFormatName == formatName)
                return true;
        return false;
    }

    /// Scans the plugin folders of every format that is on, one format after the other; not while a scan runs.
    void scanAllFormats()
    {
        if (scan != nullptr)
            return;

        scannedShown = host->numScanned();
        scanOutcome.clear();
        scanProgressed (0.0, {});
        scan = std::make_unique<audium::PluginListScan> (
            *host, audium::PluginListScan::scannableFormats (*host),
            [this] (double fraction, const juce::String& pluginName) { scanProgressed (fraction, pluginName); },
            [this] (const audium::PluginListScan::Outcome& outcome) { scanFinished (outcome); });
    }

    /// The scan's word while it runs: how far it is and the plugin at hand.
    void scanProgressed (double fraction, const juce::String& pluginName)
    {
        showScanProgress (true);
        scanFraction = juce::jlimit (0.0, 1.0, fraction);   // the bar reads it

        // a not-scanned entry the scan replaced where it was: the list does not tell of that by itself
        if (const auto scannedNow = host->numScanned(); scannedNow != std::exchange (scannedShown, scannedNow))
            host->getList().sendChangeMessage();
        scanFile.setText (pluginName.isEmpty() ? TRANS ("Looking for plugins...") : TRANS ("Scanning ") + pluginName,
                          juce::dontSendNotification);
    }

    /// The progress row in the place of the details, and nothing started or switched meanwhile.
    void showScanProgress (bool scanning)
    {
        if (scanBar.isVisible() == scanning)
            return;

        scanBar.setVisible (scanning);
        scanFile.setVisible (scanning);
        cancelScan.setVisible (scanning);
        details.setVisible (! scanning);
        enablePlugins.setEnabled (! scanning);
        updateButtons();
    }

    void selectionChanged()
    {
        remove.setVisible (isAnySelectedRemovable());
        updateDetails();
    }

    /// No scan while plugins are off, and the list is left alone while a scan fills it.
    void updateButtons()
    {
        const auto scanning = isScanning();
        rescan.setEnabled (host->isEnabled() && ! scanning);
        scanFolders.setEnabled (host->isEnabled() && ! scanning);
        clear.setEnabled (host->isEnabled() && ! scanning);
        remove.setEnabled (host->isEnabled() && ! scanning);
        for (auto* formatSwitch : formatSwitches)
            formatSwitch->setEnabled (host->isEnabled() && ! scanning);
    }

    void pluginsSwitched()
    {
        refreshRows();   // what is offered changed
        table.setEnabled (host->isEnabled());
        updateButtons();
        table.repaint();
        updateStatus();
        updateDetails();
    }

    void updateStatus()
    {
        showLog.setEnabled (host->getScanLogFile().existsAsFile());   // there once something was scanned

        if (! host->isEnabled())
        {
            const auto listed = host->getList().getNumTypes();
            juce::String text (TRANS ("Plugins are off: none is offered as an insert, and a project opens without them"));
            if (listed > 0)
                text << " (" << listed << TRANS (" in the list") << ")";
            status.setText (text, juce::dontSendNotification);
            return;
        }

        using Availability = audium::PluginHost::Availability;
        int effects = 0, instruments = 0, formatMissing = 0, formatOff = 0, needLoweredSecurity = 0, notScanned = 0, removed = 0;
        for (const auto& description : host->getList().getTypes())
        {
            if (host->isRemoved (description)) { ++removed; continue; }
            notScanned += audium::PluginHost::isScanned (description) ? 0 : 1;
            if (description.isInstrument) { ++instruments; continue; }
            switch (host->availabilityOf (description))
            {
                case Availability::available:      ++effects; break;
                case Availability::formatMissing:  ++formatMissing; break;
                case Availability::disabled:       break;
                case Availability::formatDisabled: ++formatOff; break;
                case Availability::removed:        break;
                case Availability::needsLoweredSecurity: ++needLoweredSecurity; break;
            }
        }
        const auto skipped = host->getList().getBlacklistedFiles().size();

        if (host->getList().getNumTypes() == 0 && skipped == 0 && ! isScanning())
        {
            status.setText (TRANS ("No plugins listed yet: Scan looks through the plugin folders"), juce::dontSendNotification);
            return;
        }

        juce::String text;
        text << effects << (effects == 1 ? TRANS (" effect") : TRANS (" effects")) << TRANS (" available as inserts");
        if (notScanned > 0)
            text << " (" << notScanned << TRANS (" not scanned") << ")";
        if (instruments > 0)
            text << ", " << instruments << (instruments == 1 ? TRANS (" instrument") : TRANS (" instruments")) << TRANS (" (not hosted)");
        if (formatMissing > 0)
            text << ", " << formatMissing << TRANS (" in a format not hosted here");
        if (formatOff > 0)
            text << ", " << formatOff << TRANS (" in a format that is turned off");
        if (needLoweredSecurity > 0)
            text << ", " << needLoweredSecurity << (needLoweredSecurity == 1 ? TRANS (" needs") : TRANS (" need")) << TRANS (" lowered security settings");
        if (removed > 0)
            text << ", " << removed << TRANS (" removed");
        if (skipped > 0)
            text << ", " << skipped << (skipped == 1 ? TRANS (" file skipped") : TRANS (" files skipped")) << TRANS (" (crashed, timed out or did not load when scanned)");
        status.setText (text, juce::dontSendNotification);
    }

    std::shared_ptr<audium::PluginHost> host;
    std::function<void (bool)> onEnabledChanged;
    std::function<void (const juce::String&, bool)> onFormatEnabledChanged;
    int scannedShown = 0;   // how many scanned entries the rows had when the scan last reported
    juce::String scanOutcome;   // what the last scan did, shown under the list until the selection changes
    Rows rows { *this };
    juce::TableListBox table;
    juce::TextButton rescan, clear, remove, scanFolders;
    struct Listed
    {
        juce::PluginDescription description;
        bool offered;   // as an insert
    };
    std::vector<Listed> listed;       // the host's list as the rows show it (refreshRows)
    juce::StringArray skippedFiles;   // and the files it skips, the rows after those
    mutable std::optional<std::map<juce::String, juce::File>> components;   // Audio Unit codes to component bundles, read when first asked for
    std::map<juce::String, SkippedEntry> skippedEntries;
    juce::Label details;
    double scanFraction = 0.0;
    juce::ProgressBar scanBar { scanFraction };
    juce::Label scanFile;
    juce::TextButton cancelScan;
    juce::Label status;
    juce::ToggleButton enablePlugins;
    juce::OwnedArray<juce::ToggleButton> formatSwitches;   // one per format, where the formats are switchable
    juce::ScopedMessageBox scanResult;
    juce::TextButton showLog;
    std::unique_ptr<audium::PluginListScan> scan;   // the one that runs, if one does

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginManagerComponent)
};
