//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Lola uses a GPL/commercial licence - see LICENCE.md for details.

#include "AudiumApplication.h"
#include "Interface/Components/MainComponent.h"
#include "Engine/Resource/AudioResourceContainer.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Factory/AudiumFactory.h"
#include "Application/AudiumMenuModel.h"
#include "Util/EngineAccess.h"
#include "Util/Preferences.h"
#include "AudiumMainWindow.h"
#include "Interface/Dialogs/SettingsDialog.h"
#include "Interface/Dialogs/AboutDialog.h"
#include "Interface/Dialogs/FloatingToolWindow.h"


AudiumApplication& AudiumApplication::getApp()
{
    AudiumApplication* const app = dynamic_cast<AudiumApplication*> (JUCEApplication::getInstance());
    jassert (app != nullptr);
    return *app;
}

juce::ApplicationCommandManager& AudiumApplication::getCommandManager()
{
    auto* cm = AudiumApplication::getApp().commandManager.get();
    jassert (cm != nullptr);
    return *cm;
}

void AudiumApplication::initialise (const juce::String& commandLine)
{
    LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    
    Preferences::init(getApplicationName());
    
    recentFiles.setMaxNumberOfItems(20);
    if (Preferences::valueExists(PreferenceKeys::recentFiles)) {
        recentFiles.restoreFromString (Preferences::getValue (PreferenceKeys::recentFiles));
        recentFiles.removeNonExistentFiles();
    }
    
    initCommandManager();
    
    // create audium engine
    audiumEngine = audium::AudiumFactory::createAudiumEngine();
    audiumEngine->initialise();


    initialOpenDirectory = initialSaveDirectory = File::getSpecialLocation (File::userDocumentsDirectory);
    
    if (Preferences::valueExists(PreferenceKeys::initialOpenDirectory))
        initialOpenDirectory = juce::File(Preferences::getValue(PreferenceKeys::initialOpenDirectory));
    
    if (Preferences::valueExists(PreferenceKeys::initialSaveDirectory))
        initialSaveDirectory = juce::File(Preferences::getValue(PreferenceKeys::initialSaveDirectory));
    
    
    mainWindow.reset (new AudiumMainWindow (getApplicationName(), audiumEngine));
    
    
    // do further initialisation in a moment when the message loop has started
    triggerAsyncUpdate();
}

void AudiumApplication::handleAsyncUpdate()
{
    menuModel.reset (new AudiumMenuModel());
    
#if JUCE_MAC
    rebuildAppleMenu();
    appleMenuRebuildListener = std::make_unique<AppleMenuRebuildListener>();
#endif
    
    if (Preferences::valueExists(PreferenceKeys::defaultFile)) {
        const auto file = juce::File(Preferences::getValue(PreferenceKeys::defaultFile));
        openFile(juce::File(Preferences::getValue(PreferenceKeys::defaultFile)));
    }
    
    if (!audiumEngine->getCurrentFile().exists()) {
        audiumEngine->createNewProject();
    }
    updateUI();
    
}

void AudiumApplication::shutdown()
{
    // Add your application's shutdown code here..
    
    updateSettings();

#if JUCE_MAC
    MenuBarModel::setMacMainMenu (nullptr);
#endif
    
    menuModel.reset();
    commandManager.reset();
    
    mainWindow = nullptr; // (deletes our window)
    
    audiumEngine->uninitialise();
    audiumEngine = nullptr;
    
}

void AudiumApplication::askToSaveIfDirtyAndInvoke(std::function<void ()> callback)
{
    jassert(callback != nullptr);
    
    if (!audiumEngine->getUndoManager()->canUndo())
    {
        callback();
    }
    else
    {
        juce::String docName = "Untitled";
        if (audiumEngine->getCurrentFile().existsAsFile())
            docName = audiumEngine->getCurrentFile().getFileName();
        

        auto options = MessageBoxOptions::makeOptionsYesNoCancel (MessageBoxIconType::QuestionIcon,
                                                                  TRANS ("Save the changes?"),
                                                                  TRANS ("Do you want to save the changes to \"")
                                                                      + docName + "\"?",
                                                                  TRANS ("Save"),
                                                                  TRANS ("Discard changes"),
                                                                  TRANS ("Cancel"));
        
        // -> std::function<void (int)> callback
        juce::NativeMessageBox::showAsync(options, [this, callback] (int result)
        {
            if (result == 0)
            {
                if (audiumEngine->getCurrentFile().existsAsFile())
                {
                    // pass on the callback [capture]
                    saveProject([callback](bool success){ if (success) callback();});
                }
                else
                {
                    // pass on the callback [capture]
                    saveProjectAs([callback](bool success){ if (success) callback();});
                }
            }
            else if (result == 1)
            {
                callback();
            }
        });
    }
}

void AudiumApplication::systemRequestedQuit()
{
    // This is called when the app is being asked to quit: you can ignore this
    // request and let the app carry on running, or call quit() to allow the app to close.
    
    askToSaveIfDirtyAndInvoke([](void) {
        quit();
    });
}

void AudiumApplication::anotherInstanceStarted (const juce::String& commandLine)
{
    // When another instance of the app is launched while this one is running,
    // this method is invoked, and the commandLine parameter tells you what
    // the other instance's command-line arguments were.
}

void AudiumApplication::initCommandManager()
{
    commandManager.reset (new ApplicationCommandManager());
    commandManager->registerAllCommandsForTarget (this);
}

MenuBarModel* AudiumApplication::getMenuModel()
{
    return menuModel.get();
}

StringArray AudiumApplication::getMenuNames()
{
    StringArray currentMenuNames { "File", "Edit", "View"};
    return currentMenuNames;
}

PopupMenu AudiumApplication::createMenu (const String& menuName)
{
    if (menuName == "File")
        return createFileMenu();

    if (menuName == "Edit")
        return createEditMenu();

    if (menuName == "View")
        return createViewMenu();

    jassertfalse; // names have changed?
    return {};
}

int recentProjectsBaseID = 100;


PopupMenu AudiumApplication::createFileMenu()
{
    PopupMenu menu;
    menu.addCommandItem (commandManager.get(), CommandIDs::newProject);
    menu.addSeparator();
    menu.addCommandItem (commandManager.get(), CommandIDs::openProject);

    {
        PopupMenu recentFilesMenu;
        recentFiles.createPopupMenuItems (recentFilesMenu, recentProjectsBaseID, true, true);
        // std::cout << "recentFiles.createPopupMenuItems" << std::endl;
        if (recentFilesMenu.getNumItems() > 0) {
            recentFilesMenu.addSeparator();
            recentFilesMenu.addCommandItem (commandManager.get(), CommandIDs::clearRecentFiles);
        }

        menu.addSubMenu ("Open Recent", recentFilesMenu);
    }

    menu.addSeparator();
    menu.addCommandItem (commandManager.get(), CommandIDs::saveProject);
    menu.addCommandItem (commandManager.get(), CommandIDs::saveProjectAs);
    menu.addCommandItem (commandManager.get(), CommandIDs::defaultProject);
    menu.addSeparator();
    
    menu.addCommandItem (commandManager.get(), CommandIDs::bounceProject);
    menu.addSeparator();

   #if ! JUCE_MAC
    menu.addCommandItem (commandManager.get(), CommandIDs::showAboutWindow);
    menu.addCommandItem (commandManager.get(), CommandIDs::showSettingsWindow);
//    menu.addCommandItem (commandManager.get(), CommandIDs::checkForNewVersion);
    menu.addSeparator();
    menu.addCommandItem (commandManager.get(), StandardApplicationCommandIDs::quit);
   #endif

    return menu;
}

PopupMenu AudiumApplication::createEditMenu()
{
    PopupMenu menu;
    menu.addCommandItem (commandManager.get(), StandardApplicationCommandIDs::undo);
    menu.addCommandItem (commandManager.get(), StandardApplicationCommandIDs::redo);
    menu.addSeparator();
    menu.addCommandItem (commandManager.get(), StandardApplicationCommandIDs::cut);
    menu.addCommandItem (commandManager.get(), StandardApplicationCommandIDs::copy);
    menu.addCommandItem (commandManager.get(), StandardApplicationCommandIDs::paste);
    menu.addCommandItem (commandManager.get(), StandardApplicationCommandIDs::deselectAll);
    menu.addCommandItem (commandManager.get(), StandardApplicationCommandIDs::del);
    menu.addCommandItem (commandManager.get(), StandardApplicationCommandIDs::selectAll);
    menu.addSeparator();
    menu.addCommandItem(commandManager.get(), CommandIDs::createRegion);
    menu.addCommandItem(commandManager.get(), CommandIDs::splitRegion);
    menu.addCommandItem(commandManager.get(), CommandIDs::cleanupRegions);
    
#if AUTO_EDIT_ENABLED
    menu.addSeparator();
    menu.addCommandItem(commandManager.get(), CommandIDs::autoEdit);
#endif
    //menu.addSeparator();
    //menu.addCommandItem(commandManager.get(), CommandIDs::loopPlayList);
    return menu;
}

PopupMenu AudiumApplication::createViewMenu()
{
    PopupMenu menu;
    menu.addCommandItem (commandManager.get(), CommandIDs::toggleFullScreen);
    menu.addSeparator();
    menu.addCommandItem (commandManager.get(), CommandIDs::toggleEditArrangement);
    menu.addSeparator();
    menu.addCommandItem (commandManager.get(), CommandIDs::zoomIn);
    menu.addCommandItem (commandManager.get(), CommandIDs::zoomOut);
    menu.addCommandItem (commandManager.get(), CommandIDs::pageLeft);
    menu.addCommandItem (commandManager.get(), CommandIDs::pageRight);
    menu.addSeparator();
    menu.addCommandItem (commandManager.get(), CommandIDs::followTransport);

    return menu;
}

PopupMenu AudiumApplication::createExtraAppleMenuItems()
{
    PopupMenu menu;
    menu.addCommandItem (commandManager.get(), CommandIDs::showAboutWindow);
    menu.addSeparator();
    menu.addCommandItem (commandManager.get(), CommandIDs::showSettingsWindow);
    return menu;
}


void AudiumApplication::handleMainMenuCommand (int menuItemID)
{
    if (menuItemID >= recentProjectsBaseID && menuItemID < (recentProjectsBaseID + 100)) {
        askToSaveIfDirtyAndInvoke([this, menuItemID](void) {
            openFile (recentFiles.getFile (menuItemID - recentProjectsBaseID));
        
            // will update the open recent menu
            commandManager->commandStatusChanged();
            
        });
    }
}

void AudiumApplication::getAllCommands (Array <CommandID>& commands)
{
    JUCEApplication::getAllCommands (commands);

    const CommandID ids[] = {   CommandIDs::newProject,
                                CommandIDs::openProject,
                                CommandIDs::defaultProject,
                                CommandIDs::saveProject,
                                CommandIDs::saveProjectAs,
                                CommandIDs::showAboutWindow,
                                CommandIDs::showSettingsWindow,
                                CommandIDs::clearRecentFiles,
    };

    commands.addArray (ids, numElementsInArray (ids));
}

void AudiumApplication::getCommandInfo (CommandID commandID, ApplicationCommandInfo& result)
{
    switch (commandID)
    {
    case CommandIDs::newProject:
        result.setInfo ("New Project...", "Creates a new project", CommandCategories::general, 0);
        result.defaultKeypresses.add (KeyPress ('n', ModifierKeys::commandModifier, 0));
        break;

    case CommandIDs::openProject:
        result.setInfo ("Open...", "Opens a project", CommandCategories::general, 0);
        result.defaultKeypresses.add (KeyPress ('o', ModifierKeys::commandModifier, 0));
        break;
            
    case CommandIDs::defaultProject:
        result.setInfo ("Set current project as default", "Set current project as default", CommandCategories::general, 0);
        break;
            
    case CommandIDs::saveProject:
        result.setInfo ("Save", "Saves a project", CommandCategories::general, 0);
        result.defaultKeypresses.add (KeyPress ('s', ModifierKeys::commandModifier, 0));
        break;

    case CommandIDs::saveProjectAs:
        result.setInfo ("Save as...", "Saves the current project to a new location", CommandCategories::general, 0);
        result.defaultKeypresses.add ({ 's', ModifierKeys::commandModifier | ModifierKeys::shiftModifier, 0 });
        break;
    case CommandIDs::showAboutWindow:
        result.setInfo ("About", "Shows the 'About' page.", CommandCategories::general, 0);
        break;
    case CommandIDs::showSettingsWindow:
        result.setInfo ("Settings...", "Shows the 'Settings' dialog.", CommandCategories::general, 0);
        result.defaultKeypresses.add (KeyPress (',', ModifierKeys::commandModifier, 0));
        break;
            
    case CommandIDs::checkForNewVersion:
        result.setInfo ("Check for New Version...", "Checks the web server for a new version", CommandCategories::general, 0);
        break;
    case CommandIDs::clearRecentFiles:
        result.setInfo ("Clear Recent Files", "Clears all recent files from the menu", CommandCategories::general, 0);
        result.setActive (recentFiles.getNumFiles() > 0);
        break;

    default:
        JUCEApplication::getCommandInfo (commandID, result);
        break;
    }
}

bool AudiumApplication::perform (const InvocationInfo& info)
{
    switch (info.commandID)
    {
        case CommandIDs::newProject:
            createNewProject();
            break;
        case CommandIDs::openProject:
            askUserToOpenFile();
            break;
        case CommandIDs::defaultProject:
            if (audiumEngine->getCurrentFile() != File()) {
                Preferences::setValue(PreferenceKeys::defaultFile, audiumEngine->getCurrentFile().getFullPathName());
            }
            else {
                Preferences::removeKey(PreferenceKeys::defaultFile);
            }
            break;
        case CommandIDs::saveProject:
            if (audiumEngine->getCurrentFile() == File()) {
                saveProjectAs(nullptr);
            }
            else {
                saveProject(nullptr);
            }
            break;
        case CommandIDs::saveProjectAs:
            saveProjectAs(nullptr);
            break;
        case CommandIDs::showAboutWindow:
            showAboutWindow();
            break;
        case CommandIDs::showSettingsWindow:
            showSettingsDialog();
            break;
        case CommandIDs::clearRecentFiles:
            clearRecentFiles();
            break;

        default:
            return JUCEApplication::perform (info);
    }

    return true;
}


#if JUCE_MAC
 void AudiumApplication::rebuildAppleMenu()
 {
     auto extraAppleMenuItems = createExtraAppleMenuItems();

     // workaround broken "Open Recent" submenu: not passing the
     // submenu's title here avoids the defect in JuceMainMenuHandler::addMenuItem
     MenuBarModel::setMacMainMenu (menuModel.get(), &extraAppleMenuItems); //, "Open Recent");
 }
#endif


void AudiumApplication::createNewProject()
{
    askToSaveIfDirtyAndInvoke([this](void){
        audiumEngine->cleanup();
        audiumEngine->createNewProject();
        updateUI();
    });
}

void AudiumApplication::askUserToOpenFile()
{
    askToSaveIfDirtyAndInvoke([this](void) {
    
        audiumEngine->cleanup(); // clear old project
        updateUI();
        
        // open chooser...
        chooser = std::make_unique<juce::FileChooser> ("Open File", initialOpenDirectory, "*" + String(audium::AudiumEngine::projectFileExtension));
        auto flags = FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles;

        chooser->launchAsync (flags, [this] (const FileChooser& fc) {
            openFile(fc.getResult());
        });
        
    });
}

void AudiumApplication::openFile(juce::File file)
{
    audiumEngine->openFile(file, [this, file](bool success, std::string error) {
        if (success) {
            updateUI();
            initialOpenDirectory = file.getParentDirectory();
            
            RecentlyOpenedFilesList::registerRecentFileNatively (file);
            recentFiles.addFile (file);
            updateSettings();
        }
        else {
            NativeMessageBox::showMessageBoxAsync(MessageBoxIconType::WarningIcon,
                                                  "Error",
                                                  "Failed to open " + file.getFileName() +"\n\n" + String(error));
        }
    });
}

void AudiumApplication::saveProjectAs(std::function<void (bool)> callback)
{
    chooser = std::make_unique<FileChooser> (("Save As..."), initialSaveDirectory, "*" + String(audium::AudiumEngine::projectFileExtension));
    auto flags = FileBrowserComponent::saveMode
               | FileBrowserComponent::canSelectFiles
               | FileBrowserComponent::warnAboutOverwriting;

    chooser->launchAsync (flags, [this, callback] (const FileChooser& fc)
    {
        auto file = fc.getResult();
        saveProjectToFile(file, callback);
        RecentlyOpenedFilesList::registerRecentFileNatively (file);
        recentFiles.addFile (file);
        updateSettings();
    });

}

void AudiumApplication::saveProjectToFile(juce::File file, std::function<void (bool)> callback)
{
    audiumEngine->saveFile(file, [this, file, callback] (bool success, std::string error) {
        if (success) {
            initialSaveDirectory = file.getParentDirectory();
            NullCheckedInvocation::invoke (callback, true);
        }
        else {
            NativeMessageBox::showMessageBoxAsync(MessageBoxIconType::WarningIcon,
                                                  "Error",
                                                  "Failed to save " + file.getFileName() +"\n\n" + String(error));
            NullCheckedInvocation::invoke (callback, false);
        }
    });
}

void AudiumApplication::saveProject(std::function<void (bool)> callback)
{
    saveProjectToFile(audiumEngine->getCurrentFile(), callback);
}



void AudiumApplication::updateUI()
{
    auto comp = dynamic_cast<MainComponent*>(mainWindow->getContentComponent());
    if (comp != nullptr)
    {
        comp->rebuildUI();
        comp->updateUI();
    }
}

void AudiumApplication::showAboutWindow()
{
    auto w = 600;
    auto h = 400;
    if (aboutWindow != nullptr)
        aboutWindow->toFront (true);
    else
        new FloatingToolWindow ({}, {}, new AboutWindowComponent(),
                                aboutWindow, false,
                                w, h, w, h, w, h);
}

void AudiumApplication::showSettingsDialog()
{
    if (settingsDialog == nullptr)
        settingsDialog = std::make_shared<SettingsDialog>(audiumEngine);
    settingsDialog->invoke(mainWindow->getContentComponent());
}

void AudiumApplication::clearRecentFiles()
{
    recentFiles.clear();
    recentFiles.clearRecentFilesNatively();
    updateSettings();
    menuModel->menuItemsChanged();
}

void AudiumApplication::updateSettings()
{
    Preferences::setValue(PreferenceKeys::initialOpenDirectory, initialOpenDirectory.getFullPathName());
    Preferences::setValue(PreferenceKeys::initialSaveDirectory, initialSaveDirectory.getFullPathName());
    Preferences::setValue(PreferenceKeys::recentFiles, recentFiles.toString());
    Preferences::synchronize();
}
