//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Engine/AudiumEngine.h"
#include "Engine/AudioSources/Stretch/StretchBackend.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"
#include "Util/Preferences.h"

/**
 * Settings section picking the pitch-preserving time-stretch engine
 * (Time-Stretch clip mode). Signalsmith ships; the others are candidates
 * under evaluation - the choice is process-wide and takes effect by
 * restarting the audio device, so voices get prepared with the new engine.
 */
class StretchSettingsComponent : public juce::Component
{
public:
    StretchSettingsComponent (std::shared_ptr<audium::AudiumEngine> engine,
                              audium::Preferences& prefs) :
        audiumEngine (engine),
        preferences (prefs)
    {
        engineLabel = std::make_unique<juce::Label> (juce::String{}, TRANS ("Engine"));
        engineLabel->setFont (juce::FontOptions (AudiumLookAndFeel::defaultFontSize));
        addAndMakeVisible (engineLabel.get());

        engineBox = std::make_unique<juce::ComboBox>();
        for (auto candidate : audium::StretchEngines::available())
            engineBox->addItem (juce::String (audium::StretchEngines::displayName (candidate))
                                    + "  (" + audium::StretchEngines::licence (candidate) + ")",
                                itemId (candidate));
        engineBox->onChange = [this] { applyAndStore(); };
        addAndMakeVisible (engineBox.get());

        hint = std::make_unique<juce::Label> (juce::String{},
                                              TRANS ("Used by the Time-Stretch clip mode (and tempo-locked clips in that mode).\n"
                                                     "Changing the engine restarts the audio device. Exports use the same choice."));
        hint->setFont (juce::FontOptions (AudiumLookAndFeel::defaultFontSize));
        hint->setAlpha (0.6f);
        hint->setMinimumHorizontalScale (1.0f);
        addAndMakeVisible (hint.get());

        setSize (500, 120);
    }

    /// The stored engine, Signalsmith when unset or unavailable in this build.
    static audium::StretchEngine readEngine (audium::Preferences& preferences)
    {
        using namespace audium;

        if (! preferences.valueExists (PreferenceKeys::stretchEngine))
            return StretchEngine::Signalsmith;

        const auto stored = StretchEngines::fromName (preferences.getValue (PreferenceKeys::stretchEngine));
        if (stored.has_value() && StretchEngines::isAvailable (*stored))
            return *stored;

        return StretchEngine::Signalsmith;
    }

    /// Applies the stored engine process-wide (call once at startup).
    static void applyPreference (audium::Preferences& preferences)
    {
        audium::StretchEngines::setSelected (readEngine (preferences));
    }

    void refreshFromPreferences()
    {
        engineBox->setSelectedId (itemId (readEngine (preferences)), juce::dontSendNotification);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10, 5);
        auto row = r.removeFromTop (24);
        engineLabel->setBounds (row.removeFromLeft (70));
        engineBox->setBounds (row.removeFromLeft (300));
        r.removeFromTop (8);
        hint->setBounds (r.removeFromTop (40));
    }

private:
    static int itemId (audium::StretchEngine engine) { return static_cast<int> (engine) + 1; }

    void applyAndStore()
    {
        using namespace audium;

        const auto engine = static_cast<StretchEngine> (engineBox->getSelectedId() - 1);
        if (! StretchEngines::setSelected (engine))
            return;

        preferences.setValue (PreferenceKeys::stretchEngine, StretchEngines::name (engine));
        preferences.synchronize();

        // every voice re-prepares on the restart and picks the engine up
        if (auto deviceManager = audiumEngine->getAudioDeviceManager())
            deviceManager->restartLastAudioDevice();
    }

    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    audium::Preferences& preferences;

    std::unique_ptr<juce::Label> engineLabel, hint;
    std::unique_ptr<juce::ComboBox> engineBox;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StretchSettingsComponent)
};
