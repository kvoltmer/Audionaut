//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <map>
#include <optional>

#include <JuceHeader.h>

#include "Engine/Effects/Hosting/PluginHost.h"
#include "Interface/ColourIds.h"

/**
 * The Manage Plugins window: the plugins this machine's list knows, with
 * JUCE's list component to scan the platform's plugin folders (each file in
 * a worker process of this executable, so a plugin that crashes takes down
 * only that and is skipped from then on), remove entries and sort. Under
 * the list a line counts what the list holds: the effects the inserts can
 * use, the instruments (listed, not hosted) and the files the scan skips.
 *
 * Above the list a tick box turns plugins on. Off (the default) no plugin
 * is offered as an insert, none is loaded and the list cannot be scanned;
 * on, all of them are there. In a sandboxed app on macOS that includes the
 * Audio Units macOS only loads once the user agreed to lower the app's
 * security settings: it asks when the first of them loads, which a line
 * beside the box says beforehand. The choice goes to `onEnabledChanged`,
 * which stores it and tells the host.
 *
 * The rows are drawn here (Rows): an entry the inserts cannot use - an
 * instrument, a plugin of a format not hosted, a file the scan skips - is
 * greyed out, and what the selected entry is, with the reason where it is
 * not offered, stands in two lines under the list.
 *
 * The list is the PluginHost's: a scan here reaches the engine's effect
 * registry through the host, and the insert menus list the plugins at once.
 */
class PluginManagerComponent : public juce::Component,
                               private juce::ChangeListener
{
public:
    explicit PluginManagerComponent (std::shared_ptr<audium::PluginHost> host_,
                                     std::function<void (bool)> onEnabledChanged_ = {}) :
        host (std::move (host_)),
        onEnabledChanged (std::move (onEnabledChanged_)),
        list (host->getFormats(), host->getList(), host->getDeadMansPedalFile(), nullptr, true)
    {
        list.setNumberOfThreadsForScanning (1);   // one worker process at a time
        list.setTableModel (new Rows (*this));    // the list component owns it
        addAndMakeVisible (list);

        details.setJustificationType (juce::Justification::topLeft);
        details.setFont (juce::FontOptions (12.0f));
        details.setMinimumHorizontalScale (1.0f);
        addAndMakeVisible (details);

        status.setJustificationType (juce::Justification::centredLeft);
        status.setFont (juce::FontOptions (12.0f));
        status.setColour (juce::Label::textColourId, findColour (audium::defaultTextColourId).withAlpha (0.75f));
        addAndMakeVisible (status);

        enablePlugins.setButtonText (TRANS ("Plugins enabled"));
        enablePlugins.setTooltip (TRANS ("Off, Audionaut offers and loads no plugins; a project that uses one keeps the slot and its settings."));
        enablePlugins.setToggleState (host->isEnabled(), juce::dontSendNotification);
        enablePlugins.onClick = [this]
        {
            if (onEnabledChanged)
                onEnabledChanged (enablePlugins.getToggleState());
            else
                host->setEnabled (enablePlugins.getToggleState());
            pluginsSwitched();
        };
        addAndMakeVisible (enablePlugins);

        // what turning them on brings with it, where it does
        if (audium::PluginHost::isSandboxed())
        {
            hint.setText (TRANS ("macOS may ask to lower Audionaut's security settings for some Audio Units"), juce::dontSendNotification);
            hint.setTooltip (TRANS ("Some Audio Units are not built for the sandbox Audionaut runs in. macOS loads them only after "
                                    "you agree to lower the security settings for Audionaut, and asks when the first of them loads."));
            hint.setJustificationType (juce::Justification::centredRight);
            hint.setFont (juce::FontOptions (12.0f));
            hint.setColour (juce::Label::textColourId, findColour (audium::defaultTextColourId).withAlpha (0.6f));
            addAndMakeVisible (hint);
        }

        host->getList().addChangeListener (this);
        pluginsSwitched();
    }

    ~PluginManagerComponent() override
    {
        host->getList().removeChangeListener (this);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (findColour (audium::secondaryBackgroundColourId));
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced (8);
        auto top = bounds.removeFromTop (24);
        enablePlugins.setBounds (top.removeFromLeft (160));
        hint.setBounds (top);
        bounds.removeFromTop (4);
        status.setBounds (bounds.removeFromBottom (22));
        details.setBounds (bounds.removeFromBottom (38));
        bounds.removeFromBottom (4);
        list.setBounds (bounds);
    }

private:
    /// The list's rows: the plugins, then the files the scan skips. What JUCE's own model does, with
    /// the entries the inserts cannot use greyed out, and the selection told to the window.
    class Rows final : public juce::TableListBoxModel
    {
    public:
        explicit Rows (PluginManagerComponent& owner_) : owner (owner_) {}

        // the columns the list component made
        enum { nameColumn = 1, formatColumn, categoryColumn, manufacturerColumn, descriptionColumn };

        int getNumRows() override
        {
            return plugins().getNumTypes() + plugins().getBlacklistedFiles().size();
        }

        void paintRowBackground (juce::Graphics& g, int, int, int, bool rowIsSelected) override
        {
            const auto background = owner.list.findColour (juce::ListBox::backgroundColourId);
            g.fillAll (rowIsSelected ? background.interpolatedWith (owner.list.findColour (juce::ListBox::textColourId), 0.5f)
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
                else if (columnId == descriptionColumn)
                    text = TRANS ("Skipped");
            }
            else if (const auto description = owner.pluginAt (row))
            {
                usable = owner.isOffered (*description);
                switch (columnId)
                {
                    case nameColumn:         text = description->name; break;
                    case formatColumn:       text = description->pluginFormatName; break;
                    case categoryColumn:     text = description->category.isNotEmpty() ? description->category : "-"; break;
                    case manufacturerColumn: text = description->manufacturerName; break;
                    case descriptionColumn:  text = versionAndName (*description); break;
                    default: break;
                }
            }

            if (text.isEmpty())
                return;

            const auto colour = owner.list.findColour (juce::ListBox::textColourId);
            g.setColour (! usable ? colour.withMultipliedAlpha (0.4f)
                                  : columnId == nameColumn ? colour : colour.withMultipliedAlpha (0.7f));
            g.setFont (owner.list.withDefaultMetrics (juce::FontOptions ((float) height * 0.7f, juce::Font::bold)));
            g.drawFittedText (text, 4, 0, width - 6, height, juce::Justification::centredLeft, 1, 0.9f);
        }

        void cellClicked (int row, int columnId, const juce::MouseEvent& e) override
        {
            juce::TableListBoxModel::cellClicked (row, columnId, e);
            if (row >= 0 && row < getNumRows() && e.mods.isPopupMenu())
                owner.list.createMenuForRow (row).showMenuAsync (juce::PopupMenu::Options().withTargetComponent (e.originalComponent)
                                                                                           .withMousePosition()
                                                                                           .withDeletionCheck (owner.list));
        }

        void selectedRowsChanged (int) override   { owner.updateDetails(); }
        void deleteKeyPressed (int) override      { owner.list.removeSelectedPlugins(); }

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

    std::optional<juce::PluginDescription> pluginAt (int row) const
    {
        const auto types = host->getList().getTypes();
        if (row >= 0 && row < types.size())
            return types[row];
        return std::nullopt;
    }

    /// The skipped file a row stands for, "" where the row is a plugin (or none).
    juce::String skippedFileAt (int row) const
    {
        return host->getList().getBlacklistedFiles()[row - host->getList().getNumTypes()];
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
        auto& table = list.getTableListBox();
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
                     << TRANS ("Skipped: it crashed or took too long when it was scanned. Remove it from the list and the next scan tries it again.");
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

                usable = isOffered (*description);
                if (description->isInstrument)
                    text << TRANS ("An instrument: listed, but only effects are used as inserts.");
                else if (! usable)
                    text << TRANS ("Not offered as an insert: ") << audium::PluginHost::describe (host->availabilityOf (*description)) << ".";
                else if (host->loadsOutOfProcess (*description))
                    text << TRANS ("Offered as an insert; runs in a process of its own.");
                else
                    text << TRANS ("Offered as an insert.");

                if (juce::File::isAbsolutePath (description->fileOrIdentifier))
                    text << " " << description->fileOrIdentifier;
            }
        }

        details.setColour (juce::Label::textColourId, findColour (audium::defaultTextColourId).withAlpha (usable ? 0.9f : 0.6f));
        details.setText (text, juce::dontSendNotification);
    }

    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        updateStatus();
        updateDetails();   // the row may be another entry now
    }

    void pluginsSwitched()
    {
        list.setEnabled (host->isEnabled());   // no scan while plugins are off
        list.getTableListBox().repaint();      // what is offered changed
        updateStatus();
        updateDetails();
    }

    void updateStatus()
    {
        if (! host->isEnabled())
        {
            const auto listed = host->getList().getNumTypes();
            juce::String text (TRANS ("Plugins are off: none is offered as an insert or loaded"));
            if (listed > 0)
                text << " (" << listed << TRANS (" in the list") << ")";
            status.setText (text, juce::dontSendNotification);
            return;
        }

        using Availability = audium::PluginHost::Availability;
        int effects = 0, outOfProcess = 0, instruments = 0, formatMissing = 0, needLoweredSecurity = 0;
        for (const auto& description : host->getList().getTypes())
        {
            if (description.isInstrument) { ++instruments; continue; }
            switch (host->availabilityOf (description))
            {
                case Availability::available:
                    ++effects;
                    if (host->loadsOutOfProcess (description))
                        ++outOfProcess;
                    break;
                case Availability::formatMissing:  ++formatMissing; break;
                case Availability::disabled:       break;
                case Availability::needsLoweredSecurity: ++needLoweredSecurity; break;
            }
        }
        const auto skipped = host->getList().getBlacklistedFiles().size();

        juce::String text;
        text << effects << (effects == 1 ? TRANS (" effect") : TRANS (" effects")) << TRANS (" available as inserts");
        if (outOfProcess > 0)
            text << " (" << outOfProcess << (outOfProcess == 1 ? TRANS (" runs in a process of its own") : TRANS (" run in a process of their own")) << ")";
        if (instruments > 0)
            text << ", " << instruments << (instruments == 1 ? TRANS (" instrument") : TRANS (" instruments")) << TRANS (" (not hosted)");
        if (formatMissing > 0)
            text << ", " << formatMissing << TRANS (" in a format not hosted here");
        if (needLoweredSecurity > 0)
            text << ", " << needLoweredSecurity << (needLoweredSecurity == 1 ? TRANS (" needs") : TRANS (" need")) << TRANS (" lowered security settings");
        if (skipped > 0)
            text << ", " << skipped << (skipped == 1 ? TRANS (" file skipped") : TRANS (" files skipped")) << TRANS (" (crashed or timed out when scanned)");
        status.setText (text, juce::dontSendNotification);
    }

    std::shared_ptr<audium::PluginHost> host;
    std::function<void (bool)> onEnabledChanged;
    juce::PluginListComponent list;
    std::map<juce::String, SkippedEntry> skippedEntries;
    juce::Label details;
    juce::Label status;
    juce::ToggleButton enablePlugins;
    juce::Label hint;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginManagerComponent)
};
