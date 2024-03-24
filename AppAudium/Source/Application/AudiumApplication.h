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
#include "AudiumMenuModel.h"
#include "Application/AudiumCommandIDs.h"
#include "Interface/AudiumLookAndFeel.h"

class AudiumFactory;

//==============================================================================
class AudiumApplication  : public juce::JUCEApplication, private juce::AsyncUpdater
{
public:
    //==============================================================================
    AudiumApplication() = default;
    
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
    MenuBarModel* getMenuModel();

    void getAllCommands (juce::Array<CommandID>&) override;
    void getCommandInfo (CommandID commandID, ApplicationCommandInfo&) override;
    bool perform (const InvocationInfo&) override;
    
    
    StringArray getMenuNames();
    PopupMenu createMenu (const String& menuName);
    PopupMenu createFileMenu();
    PopupMenu createEditMenu();
    PopupMenu createViewMenu();
    void handleMainMenuCommand (int menuItemID);
    PopupMenu createExtraAppleMenuItems();
    
    void askToSaveIfDirtyAndInvoke(std::function<void ()> foo);
    void createNewProject();
    void askUserToOpenFile();
    void saveProjectAs(std::function<void (bool)> callback);
    void saveProject(std::function<void (bool)> callback);
    void bounceProject();
    void updateUI();
    
    
    AudiumLookAndFeel lookAndFeel;

private:

    std::unique_ptr<AudiumMainWindow> mainWindow;
    std::shared_ptr<AudiumEngine> audiumEngine;
    std::unique_ptr<juce::ApplicationCommandManager> commandManager;
    std::unique_ptr<AudiumMenuModel> menuModel;
    std::unique_ptr<juce::FileChooser> chooser;
    
    void initCommandManager();
    void handleAsyncUpdate() override;
    
    File initialSaveDirectory;
    File initialOpenDirectory;
    
    //==============================================================================
   #if JUCE_MAC
    class AppleMenuRebuildListener  : private MenuBarModel::Listener
    {
    public:
        AppleMenuRebuildListener()
        {
            if (auto* model = AudiumApplication::getApp().getMenuModel())
                model->addListener (this);
        }

        ~AppleMenuRebuildListener() override
        {
            if (auto* model = AudiumApplication::getApp().getMenuModel())
                model->removeListener (this);
        }

    private:
        void menuBarItemsChanged (MenuBarModel*) override  {}

        void menuCommandInvoked (MenuBarModel*,
                                 const ApplicationCommandTarget::InvocationInfo& info) override
        {
            if (info.commandID == CommandIDs::enableNewVersionCheck)
                Timer::callAfterDelay (50, [] { AudiumApplication::getApp().rebuildAppleMenu(); });
        }
    };

    void rebuildAppleMenu();

    std::unique_ptr<AppleMenuRebuildListener> appleMenuRebuildListener;
   #endif
    
};
