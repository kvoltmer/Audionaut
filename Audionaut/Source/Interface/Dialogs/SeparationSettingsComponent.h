//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Engine/Separation/DemucsBackend.h"
#include "Engine/Separation/DemucsModelStore.h"
#include "Interface/Dialogs/ModelDownloadThread.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"
#include "Util/Preferences.h"

/**
 * Settings section for stem separation: how many threads the separator may
 * use, and the Demucs model - whether it is installed, where, and buttons
 * to download or remove it.
 */
class SeparationSettingsComponent : public juce::Component
{
public:
    explicit SeparationSettingsComponent (audium::Preferences& prefs) :
        preferences (prefs)
    {
        muteSourceToggle = std::make_unique<juce::ToggleButton> (TRANS ("Mute the original track after separating"));
        muteSourceToggle->onClick = [this] { applyAndStore(); };
        addAndMakeVisible (muteSourceToggle.get());

        threadsLabel = std::make_unique<juce::Label> (juce::String{}, TRANS ("Threads"));
        threadsLabel->setFont (juce::FontOptions (AudiumLookAndFeel::defaultFontSize));
        addAndMakeVisible (threadsLabel.get());

        threadsBox = std::make_unique<juce::ComboBox>();
        for (auto n = 1; n <= maxThreads(); ++n)
            threadsBox->addItem (juce::String (n), n);
        threadsBox->onChange = [this] { applyAndStore(); };
        addAndMakeVisible (threadsBox.get());

        threadsHint = std::make_unique<juce::Label> (juce::String{},
                                                     TRANS ("Each thread separates one segment of the clip.\nMore threads finish sooner but use more memory."));
        threadsHint->setFont (juce::FontOptions (AudiumLookAndFeel::defaultFontSize));
        threadsHint->setAlpha (0.6f);
        threadsHint->setMinimumHorizontalScale (1.0f);
        addAndMakeVisible (threadsHint.get());

        modelLabel = std::make_unique<juce::Label> (juce::String{}, TRANS ("Model"));
        modelLabel->setFont (juce::FontOptions (AudiumLookAndFeel::defaultFontSize));
        addAndMakeVisible (modelLabel.get());

        modelStatus = std::make_unique<juce::Label>();
        modelStatus->setFont (juce::FontOptions (AudiumLookAndFeel::defaultFontSize));
        addAndMakeVisible (modelStatus.get());

        modelLocation = std::make_unique<juce::Label>();
        modelLocation->setFont (juce::FontOptions (AudiumLookAndFeel::defaultFontSize));
        modelLocation->setAlpha (0.6f);
        modelLocation->setMinimumHorizontalScale (0.5f);
        addAndMakeVisible (modelLocation.get());

        downloadButton = std::make_unique<juce::TextButton> (TRANS ("Download"));
        downloadButton->onClick = [this] { download(); };
        addAndMakeVisible (downloadButton.get());

        removeButton = std::make_unique<juce::TextButton> (TRANS ("Remove"));
        removeButton->onClick = [this] { remove(); };
        addAndMakeVisible (removeButton.get());

        revealButton = std::make_unique<juce::TextButton> (TRANS ("Show Folder"));
        revealButton->onClick = [this]
        {
            auto store = audium::DemucsModelStore::createDefault();
            store.getDirectory().createDirectory();
            store.getDirectory().revealToUser();
        };
        addAndMakeVisible (revealButton.get());

        setSize (500, 250);
    }

    /// The number of separation threads to use; physical cores by default.
    static int readThreads (audium::Preferences& preferences)
    {
        const auto fallback = maxThreads();

        if (! preferences.valueExists (audium::PreferenceKeys::separationThreads))
            return fallback;

        const auto stored = juce::String (preferences.getValue (audium::PreferenceKeys::separationThreads)).getIntValue();
        return stored >= 1 ? juce::jmin (stored, fallback) : fallback;
    }

    /// Whether the source track is muted when its stems are added; on
    /// unless explicitly disabled.
    static bool readMuteSource (audium::Preferences& preferences)
    {
        if (! preferences.valueExists (audium::PreferenceKeys::separationMuteSource))
            return true;

        return preferences.getValue (audium::PreferenceKeys::separationMuteSource) == "true";
    }

    /** Syncs the controls to the stored preferences and the model on disk. */
    void refreshFromPreferences()
    {
        threadsBox->setSelectedId (readThreads (preferences), juce::dontSendNotification);
        muteSourceToggle->setToggleState (readMuteSource (preferences), juce::dontSendNotification);
        refreshModelStatus();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10, 5);

        const int h = 23;
        const int space = h / 4;
        const int labelWidth = 70;

        muteSourceToggle->setBounds (r.removeFromTop (h));
        r.removeFromTop (space);

        auto row = r.removeFromTop (h);
        threadsLabel->setBounds (row.removeFromLeft (labelWidth));
        threadsBox->setBounds (row.removeFromLeft (70));
        r.removeFromTop (space);
        threadsHint->setBounds (r.removeFromTop (h).withTrimmedLeft (labelWidth));
        r.removeFromTop (h);

        row = r.removeFromTop (h);
        modelLabel->setBounds (row.removeFromLeft (labelWidth));
        modelStatus->setBounds (row);
        r.removeFromTop (space);
        modelLocation->setBounds (r.removeFromTop (h).withTrimmedLeft (labelWidth));
        r.removeFromTop (space);

        row = r.removeFromTop (h).withTrimmedLeft (labelWidth);
        downloadButton->setBounds (row.removeFromLeft (100));
        row.removeFromLeft (space);
        removeButton->setBounds (row.removeFromLeft (100));
        row.removeFromLeft (space);
        revealButton->setBounds (row.removeFromLeft (100));
    }

private:
    static int maxThreads()
    {
        return juce::jmax (1, juce::SystemStats::getNumPhysicalCpus());
    }

    void applyAndStore()
    {
        preferences.setValue (audium::PreferenceKeys::separationThreads, std::to_string (threadsBox->getSelectedId()));
        preferences.setValue (audium::PreferenceKeys::separationMuteSource,
                              muteSourceToggle->getToggleState() ? "true" : "false");
        preferences.synchronize();
    }

    void refreshModelStatus()
    {
        auto store = audium::DemucsModelStore::createDefault();
        const auto& model = store.getModel();
        const auto compiledIn = audium::DemucsBackend::isCompiledIn();
        const auto installed = store.isAvailable();

        juce::String status;

        if (! compiledIn)
            status = TRANS ("This build was made without stem separation.");
        else if (installed)
            status = TRANS ("Demucs htdemucs (4 stems) installed, ") + juce::String (model.expectedBytes / (1024 * 1024)) + " MB";
        else
            status = TRANS ("Demucs htdemucs (4 stems) not downloaded, ") + juce::String (model.expectedBytes / (1024 * 1024)) + " MB";

        modelStatus->setText (status, juce::dontSendNotification);
        modelLocation->setText (store.getDirectory().getFullPathName(), juce::dontSendNotification);

        downloadButton->setEnabled (compiledIn && ! installed);
        removeButton->setEnabled (installed);
    }

    void download()
    {
        ModelDownloadThread::downloadModally (audium::DemucsModelStore::createDefault(), this);
        refreshModelStatus();
    }

    void remove()
    {
        audium::DemucsModelStore::createDefault().remove();
        refreshModelStatus();
    }

    audium::Preferences& preferences;

    std::unique_ptr<juce::Label> threadsLabel, threadsHint, modelLabel, modelStatus, modelLocation;
    std::unique_ptr<juce::ComboBox> threadsBox;
    std::unique_ptr<juce::ToggleButton> muteSourceToggle;
    std::unique_ptr<juce::TextButton> downloadButton, removeButton, revealButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeparationSettingsComponent)
};
