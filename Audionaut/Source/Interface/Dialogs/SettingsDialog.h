//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once
#include <iostream>
#include <JuceHeader.h>
#include "Engine/AudiumEngine.h"
#include "Engine/Playback/PlaybackDefines.h"
#include "Application/AudiumApplication.h"
#include "Interface/Dialogs/AnalysisSettingsComponent.h"

using namespace juce;

class SettingsDialog
{

public:
    SettingsDialog(std::shared_ptr<audium::AudiumEngine> engine) :
        audiumEngine(engine)
    {
        audioDevSelComp = std::make_unique<AudioDeviceSelectorComponent>(*engine->getAudioDeviceManager().get(),
                                                                         0,     // minimum input channels
                                                                         MAX_AUDIO_CHANNELS,   // maximum input channels
                                                                         0,     // minimum output channels
                                                                         MAX_AUDIO_CHANNELS,   // maximum output channels
                                                                         false, // ability to select midi inputs
                                                                         false, // ability to select midi output device
                                                                         false, // treat channels as stereo pairs
                                                                         false); // hide advanced options
        audioDevSelComp->setSize(500, 300);

        analysisComp = std::make_unique<AnalysisSettingsComponent>(engine, AudiumApplication::getPreferences());

        const auto tabColour = LookAndFeel::getDefaultLookAndFeel().findColour(ResizableWindow::backgroundColourId);
        tabbedComp = std::make_unique<TabbedComponent>(TabbedButtonBar::TabsAtTop);
        tabbedComp->setOutline(0);
        tabbedComp->addTab(TRANS ("Audio"), tabColour, audioDevSelComp.get(), false);
        tabbedComp->addTab(TRANS ("Analysis"), tabColour, analysisComp.get(), false);
        tabbedComp->setSize(500, 340);
    }
    
    ~SettingsDialog() = default;
    
    void invoke(juce::Component* component)
    {
        invokeInternal(component);
    }
    
private:
        
    void invokeInternal(juce::Component* component)
    {
        mainComponent = component;
        asyncAlertWindow = std::make_unique<AlertWindow> (TRANS ("Settings"),
                                                          "",
                                                          MessageBoxIconType::NoIcon, mainComponent);
        analysisComp->refreshFromPreferences();
        asyncAlertWindow->addCustomComponent(tabbedComp.get());
        asyncAlertWindow->addButton (TRANS ("Close"),  1, KeyPress (KeyPress::returnKey));

        auto resultCallback = [safeThis = WeakReference<SettingsDialog> { this }, this] (int result)
        {
            if (safeThis == nullptr)
                return;

            auto& aw = *(safeThis->asyncAlertWindow);

            aw.exitModalState (result);
            aw.setVisible (false);

            if (result == 0)
                return;
        };

        asyncAlertWindow->enterModalState (true, ModalCallbackFunction::create (std::move (resultCallback)), false);

    }
    
    std::unique_ptr<AlertWindow> asyncAlertWindow;

    // Tab contents are owned here, not by the TabbedComponent, so declare them
    // ahead of it: members destruct in reverse order, and the tabbed component
    // must let go of the pages before they are destroyed.
    std::unique_ptr<juce::AudioDeviceSelectorComponent> audioDevSelComp;
    std::unique_ptr<AnalysisSettingsComponent> analysisComp;
    std::unique_ptr<TabbedComponent> tabbedComp;

    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    juce::Component *mainComponent = nullptr;
    
    
private:
    JUCE_DECLARE_WEAK_REFERENCEABLE (SettingsDialog)
};
