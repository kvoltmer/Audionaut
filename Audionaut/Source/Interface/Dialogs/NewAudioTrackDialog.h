//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <iostream>
#include <JuceHeader.h>
#include "Engine/AudiumEngine.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Interface/Components/MainComponent.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/ResourceGroup.h"
#include "Engine/Undo/UndoableContainerAction.h"

using namespace juce;

class NewAudioTrackDialog
{
    
public:
    
    void createNewAudioTrack(std::shared_ptr<audium::AudiumEngine> engine)
    {
        createNewAudioTrackInternal(engine);
    }
    
private:
    
    static String getClassNameFieldName()  { return "Name"; }
    static String getClassNameFieldChannels()  { return "Channels"; }
    
    void createNewAudioTrackInternal(std::shared_ptr<audium::AudiumEngine> engine)
    {
        audiumEngine = engine;
        asyncAlertWindow = std::make_unique<AlertWindow> (TRANS ("Create Audio Track"),
                                                          String(),
                                                          MessageBoxIconType::NoIcon, nullptr);

        

        // Channels
        asyncAlertWindow->addTextBlock ("Number of channels:");
        String channels = "2";
        asyncAlertWindow->addTextEditor (getClassNameFieldChannels(), channels, String(), false);
        
        // Name
        asyncAlertWindow->addTextBlock ("Track name:");
        auto name = juce::String("Track ") + juce::String(engine->getAudioTrackContainer()->getNumItems() + 1);
        asyncAlertWindow->addTextEditor (getClassNameFieldName(), name, String(), false);
        
        
        
        asyncAlertWindow->addButton (TRANS ("Create"), 1, KeyPress (KeyPress::returnKey));
        asyncAlertWindow->addButton (TRANS ("Cancel"), 0, KeyPress (KeyPress::escapeKey));
        

        auto resultCallback = [safeThis = WeakReference<NewAudioTrackDialog> { this }, this] (int result)
        {
            if (safeThis == nullptr)
                return;

            auto& aw = *(safeThis->asyncAlertWindow);

            aw.exitModalState (result);
            aw.setVisible (false);

            if (result == 0)
                return;

            auto audioTrackName (aw.getTextEditorContents (getClassNameFieldName()).trim());
            auto numChannels (aw.getTextEditorContents (getClassNameFieldChannels()).trim());


            if (audioTrackName.isNotEmpty() &&
                numChannels.getIntValue() > 0)
            {
                safeThis->create(audioTrackName, numChannels.getIntValue());
                return;
            }

            safeThis->createNewAudioTrackInternal(audiumEngine);
        };

        asyncAlertWindow->enterModalState (true, ModalCallbackFunction::create (std::move (resultCallback)), false);
        auto editor = asyncAlertWindow->getTextEditor(getClassNameFieldChannels());
        if (editor != nullptr)
            editor->toFront(true);
    }
    
    void create(String name, int numChannels)
    {
        auto action = std::make_unique<audium::UndoableContainerAction>(*audiumEngine->getAudioTrackContainer());
        
        auto track = audiumEngine->getAudioTrackContainer()->createNewAudioTrack(name);
        track->setColour(audiumEngine->getAudioTrackContainer()->getNewAudioTrackColour());
        track->ensureNumChannels(numChannels);
        auto numTotalChannels = audiumEngine->getAudioTrackContainer()->getNumAudioTrackChannels();
        if (numTotalChannels >= MAX_AUDIO_CHANNELS) {
            juce::String message = "Maximum number of ";
            message += juce::String(numTotalChannels);
            message += " audio channels reached.";
            juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Error", message);
        }
        
        action->storeNewState();
        auto undoManager = audiumEngine->getAudioTrackContainer()->getUndoManager();
        undoManager->perform(action.release(), "create track");
        undoManager->beginNewTransaction();
    }
    
    std::unique_ptr<AlertWindow> asyncAlertWindow;

    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    
    JUCE_DECLARE_WEAK_REFERENCEABLE (NewAudioTrackDialog)
};
