//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <algorithm>
#include <JuceHeader.h>
#include "Engine/AudiumEngine.h"
#include "Engine/Analysis/AnalysisProvider.h"
#include "Engine/Analysis/AnalysisWorker.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"
#include "Util/Preferences.h"

/**
 * Settings section configuring which analyses run automatically when an audio
 * file is added. Changes apply immediately: each click writes the preferences
 * and pushes the new configuration into the engine's AnalysisWorker.
 */
class AnalysisSettingsComponent : public juce::Component
{
public:
    AnalysisSettingsComponent (std::shared_ptr<audium::AudiumEngine> engine,
                               audium::Preferences& prefs) :
        audiumEngine (engine),
        preferences (prefs)
    {
        autoAnalysisToggle = std::make_unique<juce::ToggleButton> (TRANS ("Analyse new audio files automatically"));
        autoAnalysisToggle->onClick = [this] { applyAndStore(); };
        addAndMakeVisible (autoAnalysisToggle.get());

        for (auto type : audium::AnalysisWorker::canonicalAnalysisTypes()) {
            auto toggle = std::make_unique<juce::ToggleButton> (displayName (type));
            toggle->onClick = [this] { applyAndStore(); };
            addAndMakeVisible (toggle.get());
            typeToggles.emplace_back (type, std::move (toggle));
        }

        hintLabel = std::make_unique<juce::Label> (String{}, TRANS ("Auto Edit uses SBic and Beats (Degara)."));
        hintLabel->setFont (juce::FontOptions (AudiumLookAndFeel::defaultFontSize));
        hintLabel->setAlpha (0.6f);
        addAndMakeVisible (hintLabel.get());

        setSize (500, 200);
    }

    /** Reads the auto-analysis master switch; on unless explicitly disabled. */
    static bool readAutoAnalysisEnabled (audium::Preferences& preferences)
    {
        if (! preferences.valueExists (audium::PreferenceKeys::autoAnalysisEnabled))
            return true;

        return preferences.getValue (audium::PreferenceKeys::autoAnalysisEnabled) == "true";
    }

    /**
     * Reads the analysis types to run automatically. An absent key means the
     * Auto Edit merge analyses (the out-of-the-box default); an empty value
     * means the user unchecked every type.
     */
    static std::vector<audium::AnalysisType> readAutoAnalysisTypes (audium::Preferences& preferences)
    {
        if (! preferences.valueExists (audium::PreferenceKeys::autoAnalysisTypes))
            return audium::AnalysisProvider::getMergeAnalysisTypes();

        return audium::analysisTypesFromString (preferences.getValue (audium::PreferenceKeys::autoAnalysisTypes));
    }

    /** Syncs the toggles to the stored preferences (call before showing). */
    void refreshFromPreferences()
    {
        const auto enabled = readAutoAnalysisEnabled (preferences);
        const auto types = readAutoAnalysisTypes (preferences);

        autoAnalysisToggle->setToggleState (enabled, juce::dontSendNotification);
        for (auto& [type, toggle] : typeToggles) {
            toggle->setToggleState (std::find (types.begin(), types.end(), type) != types.end(),
                                    juce::dontSendNotification);
            toggle->setEnabled (enabled);
        }
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

        autoAnalysisToggle->setBounds (r.removeFromTop (h));
        r.removeFromTop (space);

        for (auto& [type, toggle] : typeToggles) {
            juce::ignoreUnused (type);
            toggle->setBounds (r.removeFromTop (h).withTrimmedLeft (24));
            r.removeFromTop (space);
        }

        hintLabel->setBounds (r.removeFromTop (h));
    }

    /** The label the settings dialog shows for @p type - public so messages
        pointing the user at the settings can use the same names. */
    static juce::String displayName (audium::AnalysisType type)
    {
        switch (type) {
            case audium::AnalysisType::SBic:       return TRANS ("SBic");
            case audium::AnalysisType::Onset:      return TRANS ("Onsets");
            case audium::AnalysisType::Beat:       return TRANS ("Beats (multifeature)");
            case audium::AnalysisType::BeatDegara: return TRANS ("Beats (Degara)");
        }
        jassertfalse;
        return {};
    }

private:
    void applyAndStore()
    {
        const auto enabled = autoAnalysisToggle->getToggleState();

        std::vector<audium::AnalysisType> types;
        for (auto& [type, toggle] : typeToggles) {
            toggle->setEnabled (enabled);
            if (toggle->getToggleState())
                types.push_back (type);
        }

        preferences.setValue (audium::PreferenceKeys::autoAnalysisEnabled, enabled ? "true" : "false");
        preferences.setValue (audium::PreferenceKeys::autoAnalysisTypes, audium::analysisTypesToString (types));
        preferences.synchronize();

        if (auto worker = audiumEngine->getAudioResourceContainer()->getAnalysisWorker()) {
            worker->setAutoAnalysisEnabled (enabled);
            worker->setDefaultAnalysisTypes (types);
        }
    }

    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    audium::Preferences& preferences;

    std::unique_ptr<juce::Label> hintLabel;
    std::unique_ptr<juce::ToggleButton> autoAnalysisToggle;
    std::vector<std::pair<audium::AnalysisType, std::unique_ptr<juce::ToggleButton>>> typeToggles;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnalysisSettingsComponent)
};
