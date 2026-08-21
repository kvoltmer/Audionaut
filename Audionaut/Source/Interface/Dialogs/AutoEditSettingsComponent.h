//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Engine/PlayList/ClipDynamics.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"
#include "Util/Preferences.h"

/**
 * The Settings dialog's Auto Edit tab: the crossfade applied at the joints
 * when a clip is replaced by its segments - length in milliseconds and the
 * curve type. Changes are stored immediately, like the Analysis tab's; the
 * static readers hand the stored values to AutoEditOverlayControl when it
 * builds an AutoEditConfig.
 */
class AutoEditSettingsComponent : public juce::Component
{
public:
    explicit AutoEditSettingsComponent(audium::Preferences& preferences_) :
        preferences(preferences_)
    {
        enabledToggle = std::make_unique<juce::ToggleButton>(TRANS("Cross-fade the segment joints"));
        enabledToggle->onClick = [this] { applyAndStore(); };
        addAndMakeVisible(enabledToggle.get());

        addAndMakeVisible(timeSlider);
        timeSlider.setSliderStyle(juce::Slider::LinearBar);
        timeSlider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
        timeSlider.setColour(juce::Slider::trackColourId, juce::Colours::grey.withAlpha(0.5f));
        timeSlider.setDoubleClickReturnValue(true, defaultMs);
        timeSlider.setNormalisableRange(juce::NormalisableRange<double>(0, 200, 1));
        timeSlider.setTextValueSuffix(" ms");
        timeSlider.onValueChange = [this] { applyAndStore(); };

        timeLabel.setText(TRANS("Cross-fade time"), juce::dontSendNotification);
        timeLabel.setFont(juce::FontOptions(AudiumLookAndFeel::defaultFontSize));
        timeLabel.attachToComponent(&timeSlider, true);

        addAndMakeVisible(typeCombo);
        typeCombo.addItem(TRANS("Equal power"), equalPowerId);
        typeCombo.addItem(TRANS("Linear"), linearId);
        typeCombo.onChange = [this] { applyAndStore(); };

        typeLabel.setText(TRANS("Cross-fade type"), juce::dontSendNotification);
        typeLabel.setFont(juce::FontOptions(AudiumLookAndFeel::defaultFontSize));
        typeLabel.attachToComponent(&typeCombo, true);

        setSize(500, 200);
    }

    /** Reads the crossfade master switch; on unless explicitly disabled. */
    static bool readCrossfadesEnabled(audium::Preferences& preferences)
    {
        if (! preferences.valueExists(audium::PreferenceKeys::autoEditCrossfadesEnabled))
            return true;

        return preferences.getValue(audium::PreferenceKeys::autoEditCrossfadesEnabled) == "true";
    }

    /** The stored crossfade length in seconds (the AutoEditConfig default
        when nothing is stored). */
    static double readCrossfadeSeconds(audium::Preferences& preferences)
    {
        const auto ms = juce::String(preferences.getValue(audium::PreferenceKeys::autoEditCrossfadeMs,
                                                          juce::String(defaultMs).toStdString())).getDoubleValue();
        return ms / 1000.0;
    }

    /** The stored curve exponent: equal power unless linear is chosen. */
    static double readCrossfadeCurve(audium::Preferences& preferences)
    {
        const auto type = preferences.getValue(audium::PreferenceKeys::autoEditCrossfadeType, "equalPower");
        return type == "linear" ? 1.0 : audium::ClipDynamics::defaultFadeCurve;
    }

    void refreshFromPreferences()
    {
        const auto enabled = readCrossfadesEnabled(preferences);

        enabledToggle->setToggleState(enabled, juce::dontSendNotification);
        timeSlider.setValue(readCrossfadeSeconds(preferences) * 1000.0, juce::dontSendNotification);
        typeCombo.setSelectedId(readCrossfadeCurve(preferences) == 1.0 ? linearId : equalPowerId,
                                juce::dontSendNotification);

        setControlsEnabled(enabled);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(findColour(juce::ResizableWindow::backgroundColourId));
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(10, 5);
        const auto h = 23;
        const auto space = h / 4;

        enabledToggle->setBounds(r.removeFromTop(h));
        r.removeFromTop(space);

        // half-width controls leave room for the attached labels
        auto row = r.removeFromTop(h);
        timeSlider.setBounds(row.withX(proportionOfWidth(0.5f)).withWidth(proportionOfWidth(0.4f)));
        r.removeFromTop(space);

        row = r.removeFromTop(h);
        typeCombo.setBounds(row.withX(proportionOfWidth(0.5f)).withWidth(proportionOfWidth(0.4f)));
    }

private:
    static constexpr int defaultMs = 20;
    static constexpr int equalPowerId = 1;
    static constexpr int linearId = 2;

    void applyAndStore()
    {
        const auto enabled = enabledToggle->getToggleState();
        setControlsEnabled(enabled);

        preferences.setValue(audium::PreferenceKeys::autoEditCrossfadesEnabled,
                             enabled ? "true" : "false");
        preferences.setValue(audium::PreferenceKeys::autoEditCrossfadeMs,
                             juce::String(static_cast<int>(timeSlider.getValue())).toStdString());
        preferences.setValue(audium::PreferenceKeys::autoEditCrossfadeType,
                             typeCombo.getSelectedId() == linearId ? "linear" : "equalPower");
        preferences.synchronize();
    }

    void setControlsEnabled(bool enabled)
    {
        timeSlider.setEnabled(enabled);
        timeLabel.setEnabled(enabled);
        typeCombo.setEnabled(enabled);
        typeLabel.setEnabled(enabled);
    }

    audium::Preferences& preferences;

    std::unique_ptr<juce::ToggleButton> enabledToggle;
    juce::Slider timeSlider;
    juce::Label timeLabel;
    juce::ComboBox typeCombo;
    juce::Label typeLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoEditSettingsComponent)
};
