/*
  ==============================================================================

    AudiumApplication.h
    Created: 24 Mar 2023 10:48:29am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "AudiumMainWindow.h"

class AudiumEngine;

//==============================================================================
class AudiumApplication  : public juce::JUCEApplication
{
public:
    //==============================================================================
    AudiumApplication() {}
    
    static AudiumApplication& getApp();
    static juce::ApplicationCommandManager& getCommandManager();

    const juce::String getApplicationName() override       { return ProjectInfo::projectName; }
    const juce::String getApplicationVersion() override    { return ProjectInfo::versionString; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    void initialise (const juce::String& commandLine) override;

    void shutdown() override;

    void systemRequestedQuit() override;

    void anotherInstanceStarted (const juce::String& commandLine) override;

    //==============================================================================
    //MenuBarModel* getMenuModel();

    void getAllCommands (juce::Array<CommandID>&) override;
    void getCommandInfo (CommandID commandID, ApplicationCommandInfo&) override;
    bool perform (const InvocationInfo&) override;
    
private:
    std::unique_ptr<AudiumMainWindow> mainWindow;
    std::shared_ptr<AudiumEngine> audiumEngine;
    std::unique_ptr<juce::ApplicationCommandManager> commandManager;
    
    void initCommandManager();
};
