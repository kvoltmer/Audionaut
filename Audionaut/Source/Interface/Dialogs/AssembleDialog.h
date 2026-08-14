//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <random>
#include <JuceHeader.h>
#include "Engine/AudiumEngine.h"
#include "Engine/AutoEdit/AutoEdit.h"
#include "Application/AudiumApplication.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"
#include "Util/Preferences.h"

using namespace juce;

/**
 * Asks for the length of the sequence to assemble - a minutes and a seconds
 * field, defaulting to two minutes - then rebuilds the track's playlist
 * through AutoEdit::invokeAssemble in the given mode. The mode is picked in
 * the Assemble sub menu; the last-used length is remembered in the
 * preferences.
 */
class AssembleDialog
{

public:

    void assemble(std::shared_ptr<audium::AudiumEngine> engine, audium::AssembleConfig::Mode assembleMode)
    {
        mode = assembleMode;
        assembleInternal(engine);
    }

private:

    /**
     * The message naming the affected track in bold, the length as a minutes
     * field above a seconds field, and a short mode explanation underneath.
     * Each value is a horizontal bar handled like the channel's pan slider -
     * click or drag to the wanted position. The message lives here rather
     * than in the AlertWindow because the window's message text cannot mix
     * regular and bold type.
     */
    class LengthComponent : public Component
    {
    public:
        LengthComponent(int lengthSeconds, const String& message,
                        const String& trackName, Colour trackColour,
                        const String& description) :
            message (message),
            trackName (trackName),
            trackColour (trackColour),
            minutesSlider ("Minutes Slider Font 13"),
            secondsSlider ("Seconds Slider Font 13")
        {
            setUpSlider (minutesSlider, 60, 2, TRANS ("min"));
            setUpSlider (secondsSlider, 59, 0, TRANS ("s"));

            minutesSlider.setValue (lengthSeconds / 60, dontSendNotification);
            secondsSlider.setValue (lengthSeconds % 60, dontSendNotification);

            addAndMakeVisible (descriptionLabel);
            descriptionLabel.setText (description, dontSendNotification);
            descriptionLabel.setJustificationType (Justification::topLeft);
            descriptionLabel.setColour (Label::textColourId, Colours::white.withAlpha (0.6f));

            // The message on top, then minutes above seconds - each bar keeps
            // the header's 20 pixel slider height - and the explanation
            // underneath.
            setSize (280, 132);
        }

        void paint (Graphics& g) override
        {
            // The same font as the description below, so the two texts match.
            const auto font = descriptionLabel.getFont();
            const auto colour = findColour (AlertWindow::textColourId);

            AttributedString text;
            text.setJustification (Justification::centredTop);
            text.append (message + " ", font, colour);
            text.append (trackName, font.boldened(), trackColour);

            // The alert window places this component left of its centre, so
            // the message is centred against the window, not the component.
            auto area = messageArea.toFloat();
            if (auto* window = getParentComponent())
                area.setCentre ((float) getLocalPoint (window, window->getLocalBounds().getCentre()).x,
                                area.getCentreY());

            text.draw (g, area);
        }

        void resized() override
        {
            auto area = getLocalBounds();

            messageArea = area.removeFromTop (30).reduced (2, 0);
            area.removeFromTop (6);
            minutesSlider.setBounds (area.removeFromTop (20).reduced (2, 0));
            area.removeFromTop (4);
            secondsSlider.setBounds (area.removeFromTop (20).reduced (2, 0));
            area.removeFromTop (4);
            descriptionLabel.setBounds (area);
        }

        int getLengthSeconds() const
        {
            return (int) minutesSlider.getValue() * 60 + (int) secondsSlider.getValue();
        }

    private:
        void setUpSlider (Slider& slider, int maximum, int doubleClickValue, const String& suffix)
        {
            addAndMakeVisible (slider);
            slider.setSliderStyle (Slider::LinearBar);
            slider.setColour (Slider::textBoxTextColourId, Colours::white);
            slider.setColour (Slider::trackColourId, Colours::grey.withAlpha (0.5f));
            slider.setDoubleClickReturnValue (true, doubleClickValue);
            slider.setNormalisableRange (NormalisableRange<double> (0, maximum, 1));
            slider.setTextValueSuffix (" " + suffix);
        }

        String message;
        String trackName;
        Colour trackColour;
        Rectangle<int> messageArea;

        Slider minutesSlider;
        Slider secondsSlider;
        Label descriptionLabel;
    };

    void assembleInternal(std::shared_ptr<audium::AudiumEngine> engine)
    {
        audiumEngine = engine;

        const auto title = mode == audium::AssembleConfig::Mode::Random
                               ? TRANS ("Assemble Random Sequence")
                               : TRANS ("Assemble Sequential Sequence");

        const auto description = mode == audium::AssembleConfig::Mode::Random
                                     ? TRANS ("Builds a new sequence of the chosen length from the "
                                              "track's segments, picked at random - segments may "
                                              "repeat or be left out.")
                                     : TRANS ("Builds a new sequence of the chosen length from the "
                                              "track's segments, keeping their original order and "
                                              "leaving some out to fit.");

        // The affected track is named in the message - resolved the same way
        // the assemble itself targets it later.
        audium::AssembleConfig targetConfig;
        audium::AutoEdit (audiumEngine).targetAssembleTrack (targetConfig);
        const auto track = audiumEngine->getAudioTrackContainer()->getAudioTrack (targetConfig.trackId);

        const auto message = track != nullptr
                                 ? TRANS ("Length of the new sequence for")
                                 : TRANS ("Length of the new sequence");
        const auto trackName = track != nullptr ? track->getAudioTrackName() : String();
        const auto trackColour = track != nullptr ? track->getViewState().getColour() : Colours::white;

        // The message is drawn by the custom component, where the track name
        // can be bold - the window's own message stays empty.
        asyncAlertWindow = std::make_unique<AlertWindow> (title, String(),
                                                          MessageBoxIconType::NoIcon, nullptr);

        // The last length used is offered again - two minutes until then.
        auto& preferences = AudiumApplication::getPreferences();
        const auto minutes = String(preferences.getValue(audium::PreferenceKeys::assembleMinutes, "2")).getIntValue();
        const auto seconds = String(preferences.getValue(audium::PreferenceKeys::assembleSeconds, "0")).getIntValue();

        // The window does not own custom components, so the row outlives it as
        // a member - recreated here after the old window is gone.
        lengthComponent = std::make_unique<LengthComponent> (minutes * 60 + seconds, message, trackName, trackColour, description);
        asyncAlertWindow->addCustomComponent (lengthComponent.get());

        asyncAlertWindow->addButton (TRANS ("Assemble"), 1, KeyPress (KeyPress::returnKey));
        asyncAlertWindow->addButton (TRANS ("Cancel"), 0, KeyPress (KeyPress::escapeKey));

        auto resultCallback = [safeThis = WeakReference<AssembleDialog> { this }, this] (int result)
        {
            if (safeThis == nullptr)
                return;

            auto& aw = *(safeThis->asyncAlertWindow);

            aw.exitModalState (result);
            aw.setVisible (false);

            if (result == 0)
                return;

            const auto lengthSeconds = safeThis->lengthComponent->getLengthSeconds();
            const auto duration = (double) lengthSeconds;

            if (duration > 0.0)
            {
                auto& preferences = AudiumApplication::getPreferences();
                preferences.setValue(audium::PreferenceKeys::assembleMinutes, String(lengthSeconds / 60).toStdString());
                preferences.setValue(audium::PreferenceKeys::assembleSeconds, String(lengthSeconds % 60).toStdString());

                safeThis->assembleTrack(duration);
                return;
            }

            safeThis->assembleInternal(audiumEngine);
        };

        asyncAlertWindow->enterModalState (true, ModalCallbackFunction::create (std::move (resultCallback)), false);
    }

    void assembleTrack(double durationSeconds)
    {
        audium::AutoEdit autoEdit(audiumEngine);

        audium::AssembleConfig config;
        config.mode = mode;
        config.duration = durationSeconds;

        // A fresh seed per invocation, so re-running the command arranges the
        // material anew rather than repeating the same song.
        config.seed = std::random_device{}();

        autoEdit.targetAssembleTrack(config);

        autoEdit.invokeAssemble(config, [](std::string error)
        {
            NativeMessageBox::showMessageBoxAsync(MessageBoxIconType::WarningIcon,
                                                  "Assemble",
                                                  String(error));
        });
    }

    std::unique_ptr<AlertWindow> asyncAlertWindow;

    std::unique_ptr<LengthComponent> lengthComponent;

    std::shared_ptr<audium::AudiumEngine> audiumEngine;

    audium::AssembleConfig::Mode mode = audium::AssembleConfig::Mode::Random;

    JUCE_DECLARE_WEAK_REFERENCEABLE (AssembleDialog)
};
