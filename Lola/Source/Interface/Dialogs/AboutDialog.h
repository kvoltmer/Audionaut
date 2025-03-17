//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.

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
