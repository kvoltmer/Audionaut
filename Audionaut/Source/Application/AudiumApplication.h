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
class AboutSplashScreen;
class MainComponent;

namespace audium { class UsageAnalytics; }

class AudiumApplication  : public juce::JUCEApplication,
                           private juce::AsyncUpdater,
                           private juce::ApplicationCommandManagerListener
{
public:
    AudiumApplication();
    ~AudiumApplication() override;

    static AudiumApplication& getApp();
    static juce::ApplicationCommandManager& getCommandManager();
    static audium::Preferences& getPreferences();

    const juce::String getApplicationName() override       { return ProjectInfo::projectName; }
    const juce::String getApplicationVersion() override    { return ProjectInfo::versionString; }
    const juce::String getApplicationCompanyName()         { return ProjectInfo::companyName; }
    bool moreThanOneInstanceAllowed() override             { return false; }

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

    /// the window's content component, or nullptr while the window is not up yet
    MainComponent* getMainComponent() const;

    /// captures the view state (zoom / scroll) into the engine so it gets saved with the project
    void captureUiState();

    /// applies the view state (zoom / scroll) that was loaded with the project
    void restoreUiState();

    bool fileBrowserVisible() const;

    /// stores the usage-statistics consent and suspends/resumes the analytics
    void setUsageStatisticsEnabled(bool enabled);

    /// queues an anonymous usage event; dropped unless the user has opted in
    void logUsageEvent(const juce::String& name, const juce::StringPairArray& parameters = {});

    AudiumLookAndFeel lookAndFeel;
    
    File initialSaveDirectory;
    File initialOpenDirectory;

private:

    std::unique_ptr<AudiumMainWindow> mainWindow;
    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    std::unique_ptr<juce::ApplicationCommandManager> commandManager;
    std::unique_ptr<audium::Preferences> preferences;
    std::unique_ptr<audium::UsageAnalytics> usageAnalytics;
    std::unique_ptr<AudiumMenuModel> menuModel;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::Component> aboutComponent;
    std::unique_ptr<AboutSplashScreen> splashScreen;
    std::unique_ptr<juce::Component> fileBrowserView;
    std::shared_ptr<SettingsDialog> settingsDialog;
    
    juce::RecentlyOpenedFilesList recentFiles;
    
    void initCommandManager();
    void initPreferences();

    // logs every invoked command (menus and shortcuts) as a usage event
    void applicationCommandInvoked(const juce::ApplicationCommandTarget::InvocationInfo&) override;
    void applicationCommandListChanged() override {}

    void applyAnalysisPreferences();
    void handleAsyncUpdate() override;
    
    void showAboutWindow();
    void showFileBrowserWindow();
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

