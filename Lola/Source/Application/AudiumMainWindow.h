//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Lola uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include "Engine/AudiumEngine.h"
#include "Interface/Components/MainComponent.h"

class NewRegionDialog;
class AutoEditDialog;
class ExportAudioDialog;

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

    /* Note: Be careful if you override any DocumentWindow methods - the base
       class uses a lot of them, so by overriding you might break its functionality.
       It's best to do all your work in your content component instead, but if
       you really have to override any DocumentWindow methods, make sure your
       subclass also calls the superclass's method.
    */
    
    std::shared_ptr<audium::AudiumEngine> getEngine() const { return audiumEngine; }
    
    
    //==============================================================================
    ApplicationCommandTarget* getNextCommandTarget() override;
    void getAllCommands (Array <CommandID>& commands) override;
    void getCommandInfo (CommandID commandID, ApplicationCommandInfo& result) override;
    bool perform (const InvocationInfo& info) override;
    
private:
    
    bool isSomethingSelected();
    bool canPaste();
        
    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    std::shared_ptr<MainComponent> mainComponent;
    
    std::unique_ptr<NewRegionDialog> newRegionDialog;
    std::unique_ptr<AutoEditDialog> autoEditDialog;
    std::unique_ptr<ExportAudioDialog> exportAudioDialog;
        
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudiumMainWindow)
};
