//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once
#include <memory>
#include <JuceHeader.h>

#include "AudiumMainWindow.h"
#include "AudiumMenuModel.h"
#include "Application/AudiumCommandIDs.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"
#include "Util/Preferences.h"

class SettingsDialog;

class AudiumApplication  : public juce::JUCEApplication, private juce::AsyncUpdater
{
public:
    AudiumApplication() = default;
    
    static AudiumApplication& getApp();
    static juce::ApplicationCommandManager& getCommandManager();
    static audium::Preferences& getPreferences();

    const juce::String getApplicationName() override       { return ProjectInfo::projectName; }
    const juce::String getApplicationVersion() override    { return ProjectInfo::versionString; }
    const juce::String getApplicationCompanyName()         { return ProjectInfo::companyName; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    void initialise (const juce::String& commandLine) override;

    void shutdown() override;

    void systemRequestedQuit() override;

    void anotherInstanceStarted (const juce::String& commandLine) override;

    MenuBarModel* getMenuModel();

    void getAllCommands (juce::Array<CommandID>&) override;
    void getCommandInfo (CommandID commandID, ApplicationCommandInfo&) override;
    bool perform (const InvocationInfo&) override;
    
    
    StringArray getMenuNames();
    PopupMenu createMenu (const String& menuName);
    PopupMenu createFileMenu();
    PopupMenu createEditMenu();
    PopupMenu createCreateMenu();
    PopupMenu createViewMenu();
    void handleMainMenuCommand (int menuItemID);
    PopupMenu createExtraAppleMenuItems();
    
    
    void createNewProject();
    
    void askUserToOpenFile();
    void openFile(juce::File file);
    
    bool saveProjectAs();
    bool saveProject();
    bool saveProjectToFile(juce::File file);
    void askToSaveIfDirtyAndInvoke(std::function<void ()> foo);
    
    void updateUI();
    
    AudiumLookAndFeel lookAndFeel;
    
    File initialSaveDirectory;
    File initialOpenDirectory;

private:

    std::unique_ptr<AudiumMainWindow> mainWindow;
    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    std::unique_ptr<juce::ApplicationCommandManager> commandManager;
    std::unique_ptr<audium::Preferences> preferences;
    std::unique_ptr<AudiumMenuModel> menuModel;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::Component> aboutWindow;
    std::shared_ptr<SettingsDialog> settingsDialog;
    
    juce::RecentlyOpenedFilesList recentFiles;
    
    void initCommandManager();
    void initPreferences();
    void handleAsyncUpdate() override;
    
    void showAboutWindow();
    void showSettingsDialog();
    
    void clearRecentFiles();
    
    void updateSettings();

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

