//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <iostream>
#include <JuceHeader.h>
#include "Engine/AudiumEngine.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Interface/Components/MainComponent.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioSubGroup.h"

using namespace juce;

class NewRegionDialog
{
    
public:
    
    void createNewRegion(std::shared_ptr<audium::AudiumEngine> engine)
    {
        createNewRegionInternal(engine);
    }
    
private:
    
    static String getClassNameFieldName()  { return "Region Name"; }
    
    void createNewRegionInternal(std::shared_ptr<audium::AudiumEngine> engine)
    {
        audiumEngine = engine;
        asyncAlertWindow = std::make_unique<AlertWindow> (TRANS ("Create New Region"),
                                                          TRANS ("Please enter the name for the new region"),
                                                          MessageBoxIconType::NoIcon, nullptr);

        String name;
        auto context = audium::clocks;
        auto selectedRange = audiumEngine->getAudioTrackContainer()->getAudioRegionAdapter().getSelectedRange(context);
        for (auto track : engine->getAudioTrackContainer()->getAudioTracks()) {
            if (audiumEngine->getPlayListScheduler()->isArrangementMode()) {
                if (auto item = track->getPlayListContainer()->itemAtAbsoluteRange(selectedRange, context)) {
                    auto subGroup = item->getRegion()->getAudioSubGroup();
                    name =  subGroup->getAudioRegionContainer()->getUniqueName(item->getRegion()->getName());
                    break;
                }
            } else {
                if (auto subGroup = track->getSubGroupAtAbsoluteRange(selectedRange, context)) {
                    name = subGroup->getAudioRegionContainer()->getUniqueName(subGroup->getName());
                    break;
                }
            }
        }
        
        asyncAlertWindow->addTextEditor (getClassNameFieldName(), name, String(), false);
        asyncAlertWindow->addButton (TRANS ("Create Region"),  1, KeyPress (KeyPress::returnKey));
        asyncAlertWindow->addButton (TRANS ("Cancel"),        0, KeyPress (KeyPress::escapeKey));

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
        auto editor = asyncAlertWindow->getTextEditor(getClassNameFieldName());
        if (editor != nullptr)
            editor->toFront(true);
    }
    
    void create(String name)
    {
        bool isArrangement = audiumEngine->getPlayListScheduler()->isArrangementMode();
        audiumEngine->getAudioTrackContainer()->getAudioRegionAdapter().createRegionsFromSelection(name, isArrangement);
    }
    
    std::unique_ptr<AlertWindow> asyncAlertWindow;

    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    
    JUCE_DECLARE_WEAK_REFERENCEABLE (NewRegionDialog)
};
