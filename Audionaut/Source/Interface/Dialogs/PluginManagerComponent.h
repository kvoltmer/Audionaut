//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

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
 * The list is the PluginHost's: a scan here reaches the engine's effect
 * registry through the host, and the insert menus list the plugins at once.
 */
class PluginManagerComponent : public juce::Component,
                               private juce::ChangeListener
{
public:
    explicit PluginManagerComponent (std::shared_ptr<audium::PluginHost> host_) :
        host (std::move (host_)),
        list (host->getFormats(), host->getList(), host->getDeadMansPedalFile(), nullptr, true)
    {
        list.setNumberOfThreadsForScanning (1);   // one worker process at a time
        addAndMakeVisible (list);

        status.setJustificationType (juce::Justification::centredLeft);
        status.setFont (juce::FontOptions (12.0f));
        status.setColour (juce::Label::textColourId, findColour (audium::defaultTextColourId).withAlpha (0.75f));
        addAndMakeVisible (status);

        host->getList().addChangeListener (this);
        updateStatus();
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
        status.setBounds (bounds.removeFromBottom (22));
        bounds.removeFromBottom (4);
        list.setBounds (bounds);
    }

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        updateStatus();
    }

    void updateStatus()
    {
        using Availability = audium::PluginHost::Availability;
        int effects = 0, outOfProcess = 0, instruments = 0, formatMissing = 0;
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
        if (skipped > 0)
            text << ", " << skipped << (skipped == 1 ? TRANS (" file skipped") : TRANS (" files skipped")) << TRANS (" (crashed or timed out when scanned)");
        status.setText (text, juce::dontSendNotification);
    }

    std::shared_ptr<audium::PluginHost> host;
    juce::PluginListComponent list;
    juce::Label status;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginManagerComponent)
};
