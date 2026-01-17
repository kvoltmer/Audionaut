//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once
#include <iostream>
#include <JuceHeader.h>
#include "Engine/AudiumEngine.h"
#include "Engine/Playback/PlaybackDefines.h"

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
        asyncAlertWindow = std::make_unique<AlertWindow> (TRANS ("Audio Device Settings"),
                                                          "",
                                                          MessageBoxIconType::NoIcon, mainComponent);
        asyncAlertWindow->addCustomComponent(audioDevSelComp.get());
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
    std::unique_ptr<juce::AudioDeviceSelectorComponent> audioDevSelComp;

    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    juce::Component *mainComponent = nullptr;
    
    
private:
    JUCE_DECLARE_WEAK_REFERENCEABLE (SettingsDialog)
};
