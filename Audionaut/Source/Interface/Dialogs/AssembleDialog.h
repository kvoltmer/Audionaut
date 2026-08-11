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
#include "Util/Preferences.h"

using namespace juce;

/**
 * Asks for the length of the sequence to assemble - minutes and seconds,
 * defaulting to two minutes - and the mode, sequential by default, then
 * rebuilds the track's playlist through AutoEdit::invokeAssemble. The
 * last-used length and mode are remembered in the preferences.
 */
class AssembleDialog
{

public:

    void assemble(std::shared_ptr<audium::AudiumEngine> engine)
    {
        assembleInternal(engine);
    }

private:

    /**
     * The minutes and seconds fields side by side in one row - the alert
     * window stacks its own text editors vertically, so the row is added as a
     * custom component instead. Digits only, so no negative or garbled length
     * can be entered.
     */
    class LengthComponent : public Component
    {
    public:
        LengthComponent(const String& minutes, const String& seconds)
        {
            auto setup = [this] (Label& label, TextEditor& editor,
                                 const String& text, const String& value)
            {
                label.setText (text, dontSendNotification);
                addAndMakeVisible (label);

                editor.setInputRestrictions (3, "0123456789");
                editor.setText (value);
                // Return and escape still press the window's buttons while an
                // editor has the focus.
                editor.setEscapeAndReturnKeysConsumed (false);
                addAndMakeVisible (editor);
            };

            setup (minutesLabel, minutesEditor, TRANS ("Minutes:"), minutes);
            setup (secondsLabel, secondsEditor, TRANS ("Seconds:"), seconds);

            setSize (280, 28);
        }

        void resized() override
        {
            auto area = getLocalBounds();

            auto left = area.removeFromLeft (area.getWidth() / 2).reduced (2, 0);
            minutesLabel.setBounds (left.removeFromLeft (66));
            minutesEditor.setBounds (left);

            auto right = area.reduced (2, 0);
            secondsLabel.setBounds (right.removeFromLeft (66));
            secondsEditor.setBounds (right);
        }

        int getMinutes() const { return minutesEditor.getText().trim().getIntValue(); }
        int getSeconds() const { return secondsEditor.getText().trim().getIntValue(); }

        void focusMinutes() { minutesEditor.grabKeyboardFocus(); }

    private:
        Label minutesLabel, secondsLabel;
        TextEditor minutesEditor, secondsEditor;
    };

    /**
     * The random and sequential mode choice as two check-box buttons in one
     * row. They share a radio group, so picking one always unpicks the other.
     */
    class ModeComponent : public Component
    {
    public:
        explicit ModeComponent(audium::AssembleConfig::Mode mode)
        {
            randomButton.setButtonText (TRANS ("Random"));
            sequentialButton.setButtonText (TRANS ("Sequential"));

            for (auto* button : { &randomButton, &sequentialButton })
            {
                button->setRadioGroupId (modeGroupId);
                addAndMakeVisible (button);
            }

            (mode == audium::AssembleConfig::Mode::Random ? randomButton : sequentialButton)
                .setToggleState (true, dontSendNotification);

            setSize (280, 28);
        }

        void resized() override
        {
            auto area = getLocalBounds();

            randomButton.setBounds (area.removeFromLeft (area.getWidth() / 2).reduced (2, 0));
            sequentialButton.setBounds (area.reduced (2, 0));
        }

        audium::AssembleConfig::Mode getMode() const
        {
            return randomButton.getToggleState() ? audium::AssembleConfig::Mode::Random
                                                 : audium::AssembleConfig::Mode::Sequential;
        }

    private:
        static constexpr int modeGroupId = 1;

        ToggleButton randomButton, sequentialButton;
    };

    void assembleInternal(std::shared_ptr<audium::AudiumEngine> engine)
    {
        audiumEngine = engine;

        asyncAlertWindow = std::make_unique<AlertWindow> (TRANS ("Assemble Sequence"),
                                                          TRANS ("Please enter the length of the new sequence"),
                                                          MessageBoxIconType::NoIcon, nullptr);

        // The last length and mode used are offered again - two minutes and
        // sequential until then.
        auto& preferences = AudiumApplication::getPreferences();
        const String minutes = preferences.getValue(audium::PreferenceKeys::assembleMinutes, "2");
        const String seconds = preferences.getValue(audium::PreferenceKeys::assembleSeconds, "0");

        mode = preferences.getValue(audium::PreferenceKeys::assembleMode, "sequential") == "random"
                   ? audium::AssembleConfig::Mode::Random
                   : audium::AssembleConfig::Mode::Sequential;

        // The window does not own custom components, so the rows outlive it as
        // members - recreated here after the old window is gone.
        lengthComponent = std::make_unique<LengthComponent> (minutes, seconds);
        asyncAlertWindow->addCustomComponent (lengthComponent.get());

        modeComponent = std::make_unique<ModeComponent> (mode);
        asyncAlertWindow->addCustomComponent (modeComponent.get());

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

            // What is picked in the dialog becomes the mode.
            safeThis->mode = safeThis->modeComponent->getMode();

            const auto minutes = safeThis->lengthComponent->getMinutes();
            const auto seconds = safeThis->lengthComponent->getSeconds();
            const auto duration = minutes * 60.0 + seconds;

            if (duration > 0.0)
            {
                auto& preferences = AudiumApplication::getPreferences();
                preferences.setValue(audium::PreferenceKeys::assembleMinutes, String(minutes).toStdString());
                preferences.setValue(audium::PreferenceKeys::assembleSeconds, String(seconds).toStdString());
                preferences.setValue(audium::PreferenceKeys::assembleMode,
                                     safeThis->mode == audium::AssembleConfig::Mode::Random ? "random"
                                                                                            : "sequential");

                safeThis->assembleTrack(duration);
                return;
            }

            safeThis->assembleInternal(audiumEngine);
        };

        asyncAlertWindow->enterModalState (true, ModalCallbackFunction::create (std::move (resultCallback)), false);
        lengthComponent->focusMinutes();
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

    std::unique_ptr<ModeComponent> modeComponent;
    std::unique_ptr<LengthComponent> lengthComponent;

    std::shared_ptr<audium::AudiumEngine> audiumEngine;

    audium::AssembleConfig::Mode mode = audium::AssembleConfig::Mode::Random;

    JUCE_DECLARE_WEAK_REFERENCEABLE (AssembleDialog)
};
