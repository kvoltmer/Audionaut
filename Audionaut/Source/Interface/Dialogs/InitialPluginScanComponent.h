//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <memory>

#include <JuceHeader.h>

#include "Engine/Effects/Hosting/PluginHost.h"
#include "Engine/Effects/Hosting/PluginListScan.h"
#include "Interface/ColourIds.h"

/**
 * The scan a first launch runs where the plugins are found by looking
 * through folders (VST3, LV2): a progress bar, the plugin at hand and a
 * Cancel button, in a dialog that is modal - nothing else is started
 * while the list fills. The scan begins with the component, and the dialog
 * closes itself when the scan is over or cancelled; closing the dialog
 * ends the scan as Cancel does.
 *
 * What the scan left out is not said here: Manage Plugins lists it.
 */
class InitialPluginScanComponent final : public juce::Component
{
public:
    explicit InitialPluginScanComponent (std::shared_ptr<audium::PluginHost> host_) :
        host (std::move (host_))
    {
        const auto textColour = findColour (audium::defaultTextColourId);

        hint.setText (TRANS ("Audionaut is looking for the plugins on this computer, so that they can be used as inserts. "
                             "This happens once: later, Manage Plugins in the Options menu scans for new ones."),
                      juce::dontSendNotification);
        hint.setFont (juce::FontOptions (13.0f));
        hint.setJustificationType (juce::Justification::topLeft);
        hint.setColour (juce::Label::textColourId, textColour.withAlpha (0.9f));
        addAndMakeVisible (hint);

        bar.setPercentageDisplay (false);
        addAndMakeVisible (bar);

        plugin.setText (TRANS ("Looking for plugins..."), juce::dontSendNotification);
        plugin.setFont (juce::FontOptions (12.0f));
        plugin.setMinimumHorizontalScale (1.0f);
        plugin.setColour (juce::Label::textColourId, textColour.withAlpha (0.75f));
        addAndMakeVisible (plugin);

        cancel.setButtonText (TRANS ("Cancel"));
        cancel.onClick = [this]
        {
            cancel.setEnabled (false);
            plugin.setText (TRANS ("Cancelling..."), juce::dontSendNotification);
            scan->cancel();   // it gives up the file at hand, and says that it is over as any scan does
        };
        addAndMakeVisible (cancel);

        setSize (460, 150);

        scan = std::make_unique<audium::PluginListScan> (
            *host, audium::PluginListScan::scannableFormats (*host),
            [this] (double done, const juce::String& pluginName)
            {
                fraction = juce::jlimit (0.0, 1.0, done);   // the bar reads it
                if (cancel.isEnabled())
                    plugin.setText (pluginName.isEmpty() ? TRANS ("Looking for plugins...") : TRANS ("Scanning ") + pluginName,
                                    juce::dontSendNotification);
            },
            [this] (const audium::PluginListScan::Outcome&) { close(); });
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (findColour (audium::secondaryBackgroundColourId));
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced (12);
        auto bottom = bounds.removeFromBottom (20);
        cancel.setBounds (bottom.removeFromRight (80));
        bounds.removeFromBottom (10);
        plugin.setBounds (bounds.removeFromBottom (18));
        bounds.removeFromBottom (4);
        bar.setBounds (bounds.removeFromBottom (20));
        bounds.removeFromBottom (8);
        hint.setBounds (bounds);
    }

    /// The component in its dialog, centred around `centreAround` and not shown yet: the caller makes
    /// it modal, hears through the modal state when it is over, and deletes the dialog then.
    static std::unique_ptr<juce::DialogWindow> createDialog (std::shared_ptr<audium::PluginHost> host, juce::Component* centreAround)
    {
        juce::DialogWindow::LaunchOptions options;
        auto* content = new InitialPluginScanComponent (std::move (host));
        options.content.setOwned (content);
        options.dialogTitle = TRANS ("Scanning Plugins");
        options.dialogBackgroundColour = content->findColour (audium::secondaryBackgroundColourId);
        options.componentToCentreAround = centreAround;
        options.escapeKeyTriggersCloseButton = false;   // a stray Escape does not end the one scan
        options.useNativeTitleBar = true;
        options.resizable = false;
        return std::unique_ptr<juce::DialogWindow> (options.create());
    }

private:
    void close()
    {
        if (auto* dialog = findParentComponentOfClass<juce::DialogWindow>())
            dialog->exitModalState (0);
    }

    std::shared_ptr<audium::PluginHost> host;
    juce::Label hint, plugin;
    double fraction = 0.0;
    juce::ProgressBar bar { fraction };
    juce::TextButton cancel;
    std::unique_ptr<audium::PluginListScan> scan;   // last: gone before anything it reports to

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InitialPluginScanComponent)
};
