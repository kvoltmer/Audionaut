//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Application/UsageAnalytics.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"
#include "Util/Preferences.h"

/**
 * The Settings dialog's Privacy tab: the opt-in for anonymous usage
 * statistics. The toggle is stored immediately, like the other tabs'
 * settings; onChanged lets the application suspend or resume the
 * analytics right away.
 */
class PrivacySettingsComponent : public juce::Component
{
public:
    PrivacySettingsComponent(audium::Preferences& preferences_,
                             std::function<void(bool)> onChanged_) :
        preferences(preferences_),
        onChanged(std::move(onChanged_))
    {
        enabledToggle = std::make_unique<juce::ToggleButton>(TRANS("Share anonymous usage statistics"));
        enabledToggle->onClick = [this] { applyAndStore(); };
        addAndMakeVisible(enabledToggle.get());

        descriptionLabel.setText(TRANS("Helps improve Audionaut by reporting app launches and feature "
                                       "usage. No audio, project content or personal data is collected; "
                                       "the install is identified only by a random ID."),
                                 juce::dontSendNotification);
        descriptionLabel.setFont(juce::FontOptions(AudiumLookAndFeel::defaultFontSize));
        descriptionLabel.setColour(juce::Label::textColourId,
                                   findColour(juce::Label::textColourId).withAlpha(0.7f));
        descriptionLabel.setJustificationType(juce::Justification::topLeft);
        addAndMakeVisible(descriptionLabel);

        setSize(500, 200);
    }

    void refreshFromPreferences()
    {
        enabledToggle->setToggleState(audium::UsageAnalytics::isEnabled(preferences),
                                      juce::dontSendNotification);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(findColour(juce::ResizableWindow::backgroundColourId));
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(10, 5);
        const auto h = 23;

        enabledToggle->setBounds(r.removeFromTop(h));
        r.removeFromTop(h / 4);

        descriptionLabel.setBounds(r.removeFromTop(3 * h).withTrimmedLeft(h + 4));
    }

private:
    void applyAndStore()
    {
        // the preference is stored by the application applying the change,
        // so the toggle and the engine can't get out of sync
        if (onChanged != nullptr)
            onChanged(enabledToggle->getToggleState());
    }

    audium::Preferences& preferences;
    std::function<void(bool)> onChanged;

    std::unique_ptr<juce::ToggleButton> enabledToggle;
    juce::Label descriptionLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PrivacySettingsComponent)
};
