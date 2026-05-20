//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include "Engine/AudiumEngine.h"
#include "Interface/Components/MainComponent.h"

class NewRegionDialog;
class AutoEditDialog;
class ExportAudioDialog;
class NewAudioTrackDialog;

//==============================================================================
/*
    This class implements the desktop window that contains an instance of
    our MainComponent class.
*/
class AudiumMainWindow    : public juce::DocumentWindow, public juce::ApplicationCommandTarget
{
public:
    AudiumMainWindow (juce::String name, std::shared_ptr<audium::AudiumEngine> audiumEngine);
    ~AudiumMainWindow() override;

    void closeButtonPressed() override;
    
    std::shared_ptr<audium::AudiumEngine> getEngine() const { return audiumEngine; }
    
    ApplicationCommandTarget* getNextCommandTarget() override;
    void getAllCommands (Array <CommandID>& commands) override;
    void getCommandInfo (CommandID commandID, ApplicationCommandInfo& result) override;
    bool perform (const InvocationInfo& info) override;
    
private:
    
    bool anythingSelected();
    bool canPaste();
        
    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    std::shared_ptr<MainComponent> mainComponent;
    
    std::unique_ptr<NewRegionDialog> newRegionDialog;
    std::unique_ptr<AutoEditDialog> autoEditDialog;
    std::unique_ptr<ExportAudioDialog> exportAudioDialog;
    std::unique_ptr<NewAudioTrackDialog> newAudioTrackDialog;
        
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudiumMainWindow)
};
