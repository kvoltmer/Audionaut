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
 * defaulting to two minutes - then rebuilds the track's playlist through
 * AutoEdit::invokeAssemble in the invoked mode.
 */
class AssembleDialog
{

public:

    void assemble(std::shared_ptr<audium::AudiumEngine> engine, audium::AssembleConfig::Mode mode)
    {
        assembleInternal(engine, mode);
    }

private:

    static String getMinutesFieldName()  { return "Minutes"; }
    static String getSecondsFieldName()  { return "Seconds"; }

    static String getTitle(audium::AssembleConfig::Mode mode)
    {
        return mode == audium::AssembleConfig::Mode::Random ? "Assemble Random Sequence"
                                                            : "Assemble Sequential Sequence";
    }

    void assembleInternal(std::shared_ptr<audium::AudiumEngine> engine, audium::AssembleConfig::Mode mode_)
    {
        audiumEngine = engine;
        mode = mode_;

        asyncAlertWindow = std::make_unique<AlertWindow> (TRANS (getTitle(mode)),
                                                          TRANS ("Please enter the length of the new sequence"),
                                                          MessageBoxIconType::NoIcon, nullptr);

        // The last length used is offered again, two minutes until then.
        auto& preferences = AudiumApplication::getPreferences();
        const String minutes = preferences.getValue(audium::PreferenceKeys::assembleMinutes, "2");
        const String seconds = preferences.getValue(audium::PreferenceKeys::assembleSeconds, "0");

        asyncAlertWindow->addTextBlock ("Minutes:");
        asyncAlertWindow->addTextEditor (getMinutesFieldName(), minutes, String(), false);

        asyncAlertWindow->addTextBlock ("Seconds:");
        asyncAlertWindow->addTextEditor (getSecondsFieldName(), seconds, String(), false);

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

            const auto minutes = aw.getTextEditorContents (getMinutesFieldName()).trim().getIntValue();
            const auto seconds = aw.getTextEditorContents (getSecondsFieldName()).trim().getIntValue();
            const auto duration = minutes * 60.0 + seconds;

            if (minutes >= 0 && seconds >= 0 && duration > 0.0)
            {
                auto& preferences = AudiumApplication::getPreferences();
                preferences.setValue(audium::PreferenceKeys::assembleMinutes, String(minutes).toStdString());
                preferences.setValue(audium::PreferenceKeys::assembleSeconds, String(seconds).toStdString());

                safeThis->assembleTrack(duration);
                return;
            }

            safeThis->assembleInternal(audiumEngine, mode);
        };

        asyncAlertWindow->enterModalState (true, ModalCallbackFunction::create (std::move (resultCallback)), false);
        auto editor = asyncAlertWindow->getTextEditor(getMinutesFieldName());
        if (editor != nullptr)
            editor->toFront(true);
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

    std::shared_ptr<audium::AudiumEngine> audiumEngine;

    audium::AssembleConfig::Mode mode = audium::AssembleConfig::Mode::Random;

    JUCE_DECLARE_WEAK_REFERENCEABLE (AssembleDialog)
};
