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
#include "Engine/AudiumEngine.h"
#include "Engine/AutoEdit/AutoEdit.h"
#include "Interface/Components/MainComponent.h"
#include "Interface/Dialogs/AutoEditComponent.h"

using namespace juce;

class AutoEditDialog
{
    
public:
    AutoEditDialog()
    {
        autoEditComponent.reset(new AutoEditComponent());
    }
    
    void invokeAutoEdit(std::shared_ptr<AudiumEngine> engine, std::shared_ptr<MainComponent> component)
    {
        invokeAutoEditInternal(engine, component);
    }
    
private:
    
    static String getClassNameFieldName()  { return "Auto Edit Name"; }
    
    void invokeAutoEditInternal(std::shared_ptr<AudiumEngine> engine,
                                std::shared_ptr<MainComponent> component)
    {
        audiumEngine = engine;
        mainComponent = component;
        asyncAlertWindow = std::make_unique<AlertWindow> (TRANS ("Auto Edit Parameters:"),
                                                          "",
                                                          MessageBoxIconType::NoIcon, mainComponent.get());

        asyncAlertWindow->addCustomComponent(autoEditComponent.get());
        asyncAlertWindow->addButton (TRANS ("Apply"),  1, KeyPress (KeyPress::returnKey));
        asyncAlertWindow->addButton (TRANS ("Cancel"), 0, KeyPress (KeyPress::escapeKey));
        



        auto resultCallback = [safeThis = WeakReference<AutoEditDialog> { this }, this] (int result)
        {
            if (safeThis == nullptr)
                return;

            auto& aw = *(safeThis->asyncAlertWindow);

            aw.exitModalState (result);
            aw.setVisible (false);

            if (result == 0)
                return;
            
            auto mode = autoEditComponent->getEditMode().toString().getIntValue();
            AutoEditConfig config;
            switch (mode) {
                case 1:
                    config.mode = "random";
                    break;
                case 2:
                    config.mode = "sequential";
                    break;
                default:
                    config.mode = "random";
                    break;
            }
            
            config.duration = autoEditComponent->getDuration().toString().getDoubleValue();
            config.numSegments = autoEditComponent->getNumSegments().toString().getIntValue();
            config.minSegLength = autoEditComponent->getMinSegmentLength().toString().getDoubleValue();
            config.maxSegLength = autoEditComponent->getMaxSegmentLength().toString().getDoubleValue();
        
            safeThis->autoEdit(config);
        };

        asyncAlertWindow->enterModalState (true, ModalCallbackFunction::create (std::move (resultCallback)), false);
        auto editor = asyncAlertWindow->getTextEditor(getClassNameFieldName());
        if (editor != nullptr)
            editor->toFront(true);
    }
    
    void autoEdit(const AutoEditConfig config)
    {
        audiumEngine->invokeAutoEdit(config);
    }
    
    std::unique_ptr<AlertWindow> asyncAlertWindow;
    std::unique_ptr<AutoEditComponent> autoEditComponent;

    std::shared_ptr<AudiumEngine> audiumEngine;
    std::shared_ptr<MainComponent> mainComponent;
    
    JUCE_DECLARE_WEAK_REFERENCEABLE (AutoEditDialog)
};
