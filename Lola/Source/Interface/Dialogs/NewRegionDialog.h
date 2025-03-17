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
