//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Engine/AudiumEngine.h"
#include "Engine/Link/LinkAudioDevice.h"
#include "Engine/Playback/AudioBusInterface.h"
#include "Engine/Playback/RenderThreadPool.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"
#include "Util/Preferences.h"

/**
 * The Settings dialog's Processing tab: how many threads render the live
 * mix. Tracks render in parallel on real-time threads inside the audio
 * callback; the setting is the thread count including the audio thread
 * itself, so 1 is the serial path. "Default" leaves a core for the interface
 * and disk reads. Stored immediately, like the other tabs, and applied to
 * the running device at once; the static reader hands the stored value to
 * the engine at startup.
 */
class ProcessingSettingsComponent : public juce::Component
{
public:
    ProcessingSettingsComponent (std::shared_ptr<audium::AudiumEngine> engine_, audium::Preferences& preferences_) :
        engine (std::move (engine_)),
        preferences (preferences_)
    {
        // the label keeps the look and feel's label font, the size the other tabs' controls use
        threadsLabel = std::make_unique<juce::Label> (juce::String{}, TRANS ("Processing threads"));
        addAndMakeVisible (threadsLabel.get());

        threadsBox = std::make_unique<juce::ComboBox>();
        threadsBox->addItem (TRANS ("Default") + " (" + juce::String (audium::RenderThreadPool::defaultNumWorkers() + 1) + ")", defaultItemId);
        for (auto threads = 1; threads <= maxThreads(); ++threads)
            threadsBox->addItem (juce::String (threads), threadsItemId (threads));
        threadsBox->onChange = [this] { applyAndStore(); };
        addAndMakeVisible (threadsBox.get());

        threadsHint = std::make_unique<juce::Label> (juce::String{},
                                                     TRANS ("Tracks render in parallel on real-time threads inside the audio callback.\n"
                                                            "Default leaves a core for the interface and disk reads; 1 renders everything on the audio thread.\n"
                                                            "View > Processing Load shows what each thread carries."));
        threadsHint->setFont (juce::FontOptions (AudiumLookAndFeel::defaultFontSize));
        threadsHint->setAlpha (0.6f);
        threadsHint->setMinimumHorizontalScale (1.0f);
        addAndMakeVisible (threadsHint.get());

        setSize (500, 200);
    }

    /**
     * The stored worker count for AudioBusInterface::setNumRenderThreads:
     * -1 (default) unless a thread count was chosen, then that count minus the
     * audio thread.
     */
    static int readNumRenderThreads (audium::Preferences& preferences)
    {
        const auto stored = preferences.getValue (audium::PreferenceKeys::processingThreads, "auto");
        if (stored == "auto")
            return -1;

        const auto threads = juce::String (stored).getIntValue();
        return juce::jlimit (0, maxThreads() - 1, threads - 1);
    }

    void refreshFromPreferences()
    {
        const auto workers = readNumRenderThreads (preferences);
        threadsBox->setSelectedId (workers < 0 ? defaultItemId : threadsItemId (workers + 1), juce::dontSendNotification);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (findColour (juce::ResizableWindow::backgroundColourId));
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10, 10);
        const auto h = 23;
        const auto labelWidth = 150;
        auto row = r.removeFromTop (h);
        threadsLabel->setBounds (row.removeFromLeft (labelWidth));
        threadsBox->setBounds (row.removeFromLeft (120));
        r.removeFromTop (6);
        threadsHint->setBounds (r.removeFromTop (3 * h));
    }

private:
    static constexpr int defaultItemId = 1;
    static int threadsItemId (int threads) { return threads + 1; }
    /// The most threads offered: one per physical core, at most the pool's limit (a persisted higher value is clamped on read).
    static int maxThreads() { return juce::jlimit (1, audium::RenderThreadPool::maxWorkers + 1, juce::SystemStats::getNumPhysicalCpus()); }

    void applyAndStore()
    {
        const auto selected = threadsBox->getSelectedId();
        const auto workers = selected == defaultItemId ? -1 : selected - 2;
        preferences.setValue (audium::PreferenceKeys::processingThreads,
                              workers < 0 ? std::string ("auto") : std::to_string (workers + 1));
        preferences.synchronize();

        engine->getAudioBusInterface()->setNumRenderThreads (workers);
        if (auto device = engine->getLinkAudioDevice())
            device->restartRenderThreads();
    }

    std::shared_ptr<audium::AudiumEngine> engine;
    audium::Preferences& preferences;

    std::unique_ptr<juce::Label> threadsLabel;
    std::unique_ptr<juce::ComboBox> threadsBox;
    std::unique_ptr<juce::Label> threadsHint;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProcessingSettingsComponent)
};
