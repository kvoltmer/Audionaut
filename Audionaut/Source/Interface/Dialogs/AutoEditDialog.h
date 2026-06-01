//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

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
    AutoEditDialog(std::shared_ptr<audium::AudiumEngine> audiumEngine_)
    {
        autoEditComponent = std::make_unique<AutoEditComponent>(audiumEngine_);
    }
    
    void invoke(std::shared_ptr<audium::AudiumEngine> engine, std::shared_ptr<MainComponent> component)
    {
        invokeInternal(engine, component);
    }
    
private:
    
    static String getClassNameFieldName()  { return "Auto Edit Name"; }
    
    void invokeInternal(std::shared_ptr<audium::AudiumEngine> engine,
                        std::shared_ptr<MainComponent> component)
    {
        audiumEngine = engine;
        mainComponent = component;
        asyncAlertWindow = std::make_unique<AlertWindow> (TRANS ("Auto Edit"), "",
                                                          MessageBoxIconType::NoIcon, mainComponent.get());
        asyncAlertWindow->setMessage("Analyze and automatically edit the selected audio clip.");
        asyncAlertWindow->addCustomComponent(autoEditComponent.get());
        autoEditComponent->update();
        asyncAlertWindow->addButton (TRANS ("Apply"),  1, KeyPress (KeyPress::returnKey));
        asyncAlertWindow->addButton (TRANS ("Cancel"), 0, KeyPress (KeyPress::escapeKey));

        auto resultCallback = [safeThis = WeakReference<AutoEditDialog> { this }] (int result)
        {
            if (safeThis == nullptr)
                return;

            auto& aw = *(safeThis->asyncAlertWindow);

            aw.exitModalState (result);
            aw.setVisible (false);

            if (result == 0)
                return;
        
            safeThis->apply();
        };

        asyncAlertWindow->enterModalState (true, ModalCallbackFunction::create (std::move (resultCallback)), false);
        auto editor = asyncAlertWindow->getTextEditor(getClassNameFieldName());
        if (editor != nullptr)
            editor->toFront(true);
    }
    
    void apply()
    {
        auto autoEdit = std::make_unique<audium::AutoEdit>(audiumEngine);
        autoEdit->invokeAutoEdit(autoEditComponent->getAutoEditConfig(), [](std::string error) {
#if CATCH2_TESTS
            std::cout << "error: " << error << std::endl;
#else
            NativeMessageBox::showMessageBoxAsync(MessageBoxIconType::WarningIcon,
                                                  "Error",
                                                  String(error));
#endif
        });
    }
    
    std::unique_ptr<AlertWindow> asyncAlertWindow;
    std::unique_ptr<AutoEditComponent> autoEditComponent;

    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    std::shared_ptr<MainComponent> mainComponent;
    
    JUCE_DECLARE_WEAK_REFERENCEABLE (AutoEditDialog)
};
