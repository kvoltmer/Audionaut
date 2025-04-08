//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <iostream>
#include <JuceHeader.h>
#include "Application/AudiumApplication.h"

#include "Interface/Dialogs/AboutWindowComponent.h"

using namespace juce;

class AboutDialog
{
    
public:
    AboutDialog()
    {
        aboutWindowComponent = std::make_unique<AboutWindowComponent>();
        aboutWindowComponent->setSize(500, 300);
    }
    
    ~AboutDialog() = default;
    
    
    void invoke(juce::Component* component)
    {
        invokeInternal(component);
    }
    
private:
        
    void invokeInternal(juce::Component* component)
    {
        mainComponent = component;
        asyncAlertWindow = std::make_unique<AlertWindow> (TRANS (""),
                                                          "",
                                                          MessageBoxIconType::NoIcon, mainComponent);
        asyncAlertWindow->addCustomComponent(aboutWindowComponent.get());
        asyncAlertWindow->addButton (TRANS ("Close"),  1, KeyPress (KeyPress::returnKey));

        auto resultCallback = [safeThis = WeakReference<AboutDialog> { this }, this] (int result)
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
    std::unique_ptr<AboutWindowComponent> aboutWindowComponent;

    juce::Component *mainComponent = nullptr;
    
    
private:
    JUCE_DECLARE_WEAK_REFERENCEABLE (AboutDialog)
};
