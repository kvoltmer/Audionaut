//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Interface/LookAndFeel/AudiumLookAndFeel.h"
#include "Util/Preferences.h"

/**
 * The Settings dialog's Project tab: project handling options - the startup
 * choice of reopening the last project, and clearing the pinned default
 * project ("Set current project as default"). Changes are stored
 * immediately, like the other tabs'; the static reader hands the stored
 * value to AudiumApplication::loadStartupProject().
 */
class ProjectSettingsComponent : public juce::Component
{
public:
    explicit ProjectSettingsComponent(audium::Preferences& preferences_) :
        preferences(preferences_)
    {
        openLastToggle = std::make_unique<juce::ToggleButton>(TRANS("Open last project on launch"));
        openLastToggle->onClick = [this] { applyAndStore(); };
        addAndMakeVisible(openLastToggle.get());

        descriptionLabel.setText(TRANS("Reopens the project you last worked on when the app starts. "
                                       "A project set as default takes priority."),
                                 juce::dontSendNotification);
        descriptionLabel.setFont(juce::FontOptions(AudiumLookAndFeel::defaultFontSize));
        descriptionLabel.setColour(juce::Label::textColourId,
                                   findColour(juce::Label::textColourId).withAlpha(0.7f));
        descriptionLabel.setJustificationType(juce::Justification::topLeft);
        addAndMakeVisible(descriptionLabel);

        clearDefaultButton = std::make_unique<juce::TextButton>(TRANS("Clear default project"));
        clearDefaultButton->onClick = [this] { clearDefaultProject(); };
        addAndMakeVisible(clearDefaultButton.get());

        defaultProjectLabel.setFont(juce::FontOptions(AudiumLookAndFeel::defaultFontSize));
        defaultProjectLabel.setColour(juce::Label::textColourId,
                                      findColour(juce::Label::textColourId).withAlpha(0.7f));
        defaultProjectLabel.setJustificationType(juce::Justification::topLeft);
        addAndMakeVisible(defaultProjectLabel);

        setSize(500, 200);
    }

    /** Reads the startup recall switch; on unless explicitly disabled. */
    static bool readOpenLastProjectEnabled(audium::Preferences& preferences)
    {
        if (! preferences.valueExists(audium::PreferenceKeys::openLastProjectOnLaunch))
            return true;

        return preferences.getValue(audium::PreferenceKeys::openLastProjectOnLaunch) == "true";
    }

    void refreshFromPreferences()
    {
        openLastToggle->setToggleState(readOpenLastProjectEnabled(preferences),
                                       juce::dontSendNotification);
        refreshDefaultProjectRow();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(findColour(juce::ResizableWindow::backgroundColourId));
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(10, 5);
        const auto h = 23;

        openLastToggle->setBounds(r.removeFromTop(h));
        r.removeFromTop(h / 4);

        descriptionLabel.setBounds(r.removeFromTop(2 * h).withTrimmedLeft(h + 4));
        r.removeFromTop(h);

        clearDefaultButton->setBounds(r.removeFromTop(h).removeFromLeft(160));
        r.removeFromTop(h / 4);

        defaultProjectLabel.setBounds(r.removeFromTop(2 * h));
    }

private:
    void applyAndStore()
    {
        preferences.setValue(audium::PreferenceKeys::openLastProjectOnLaunch,
                             openLastToggle->getToggleState() ? "true" : "false");
        preferences.synchronize();
    }

    void clearDefaultProject()
    {
        preferences.removeKey(audium::PreferenceKeys::defaultFile);
        preferences.synchronize();
        refreshDefaultProjectRow();
    }

    void refreshDefaultProjectRow()
    {
        const auto hasDefault = preferences.valueExists(audium::PreferenceKeys::defaultFile);

        defaultProjectLabel.setText(hasDefault ? juce::String(preferences.getValue(audium::PreferenceKeys::defaultFile))
                                               : TRANS("No default project set"),
                                    juce::dontSendNotification);

        clearDefaultButton->setEnabled(hasDefault);
    }

    audium::Preferences& preferences;

    std::unique_ptr<juce::ToggleButton> openLastToggle;
    juce::Label descriptionLabel;
    std::unique_ptr<juce::TextButton> clearDefaultButton;
    juce::Label defaultProjectLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProjectSettingsComponent)
};
