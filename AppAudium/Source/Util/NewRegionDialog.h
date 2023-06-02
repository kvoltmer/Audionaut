/*
  ==============================================================================

    NewRegionDialog.h
    Created: 1 Jun 2023 4:23:13pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <iostream>
#include <JuceHeader.h>
#include "Engine/AudiumEngine.h"

using namespace juce;

class NewRegionDialog
{
    
public:
    
    void createNewRegion(std::shared_ptr<AudiumEngine> engine)
    {
        createNewRegionInternal(engine);
    }
    
private:
    
    static String getClassNameFieldName()  { return "Region Name"; }
    
    void createNewRegionInternal(std::shared_ptr<AudiumEngine> engine)
    {
        audiumEngine = engine;
        asyncAlertWindow = std::make_unique<AlertWindow> (TRANS ("Create New Region"),
                                                          TRANS ("Please enter the name for the new region"),
                                                          MessageBoxIconType::NoIcon, nullptr);

        asyncAlertWindow->addTextEditor (getClassNameFieldName(), String(), String(), false);
        asyncAlertWindow->addButton (TRANS ("Create Region"),  1, KeyPress (KeyPress::returnKey));
        asyncAlertWindow->addButton (TRANS ("Cancel"),        0, KeyPress (KeyPress::escapeKey));
        
        //asyncAlertWindow->getTextEditor(getClassNameFieldName())->grabKeyboardFocus();


        auto resultCallback = [safeThis = WeakReference<NewRegionDialog> { this }, this] (int result)
        {
            if (safeThis == nullptr)
                return;

            auto& aw = *(safeThis->asyncAlertWindow);

            aw.exitModalState (result);
            aw.setVisible (false);

            if (result == 0)
                return;

            const String regionName (aw.getTextEditorContents (getClassNameFieldName()).trim());

            if (regionName.isNotEmpty())
            {
                safeThis->create(regionName);
                return;
            }

            safeThis->createNewRegionInternal(audiumEngine);
        };

        asyncAlertWindow->enterModalState (true, ModalCallbackFunction::create (std::move (resultCallback)), false);
    }
    
    void create(String name)
    {
        audiumEngine->getAudioRegionContainer()->createRegion(name);
    }
    
    std::unique_ptr<AlertWindow> asyncAlertWindow;

    std::shared_ptr<AudiumEngine> audiumEngine;
    
    JUCE_DECLARE_WEAK_REFERENCEABLE (NewRegionDialog)
};
