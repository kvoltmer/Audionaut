//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

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
#include "Interface/Dialogs/FloatingToolWindow.h"
#include "Interface/Dialogs/FileBrowserView.h"
#include "Interface/Dialogs/AboutWindowComponent.h"

using namespace audium;

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

audium::Preferences& AudiumApplication::getPreferences()
{
    auto* prefs = AudiumApplication::getApp().preferences.get();
    jassert (prefs != nullptr);
    return *prefs;
}

void AudiumApplication::initialise (const juce::String& commandLine)
{
    LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    
    initPreferences();
    initCommandManager();
    
    // create audium engine
    audiumEngine = audium::AudiumFactory::createAudiumEngine();
    audiumEngine->initialise();


    initialOpenDirectory = File::getSpecialLocation (File::userMusicDirectory);
    initialSaveDirectory = File(File::getSpecialLocation (File::userMusicDirectory).getFullPathName() +
                                File::getSeparatorString() +
                                getApplicationName());
    if (initialSaveDirectory.exists() == false) {
        initialSaveDirectory.createDirectory();
    }
        
    
    if (getPreferences().valueExists(PreferenceKeys::initialOpenDirectory))
        initialOpenDirectory = juce::File(getPreferences().getValue(PreferenceKeys::initialOpenDirectory));
    
    if (getPreferences().valueExists(PreferenceKeys::initialSaveDirectory))
        initialSaveDirectory = juce::File(getPreferences().getValue(PreferenceKeys::initialSaveDirectory));
    
    
    
    // try to load project from command line
    if (! commandLine.trim().startsWithChar ('-')) {
        ArgumentList list ({}, commandLine);

        for (auto& arg : list.arguments) {
            std::cout << "YO: " << arg.resolveAsFile().getFullPathName() << std::endl;
            openFile (arg.resolveAsFile());
        }
    }
    


    
    // do further initialisation in a moment when the message loop has started
    triggerAsyncUpdate();
}

void AudiumApplication::handleAsyncUpdate()
{
    menuModel = std::make_unique<AudiumMenuModel>();
    
#if JUCE_MAC
    rebuildAppleMenu();
    appleMenuRebuildListener = std::make_unique<AppleMenuRebuildListener>();
#endif
    mainWindow.reset (new AudiumMainWindow (getApplicationName(), audiumEngine));

    auto browserOpen = getPreferences().getValue(PreferenceKeys::browserWindowOpen);
    if (browserOpen == "true")
        showFileBrowserWindow();
    
    // try to load default project from prefs
    if (!audiumEngine->getCurrentProjectFile().exists()) {
        if (getPreferences().valueExists(PreferenceKeys::defaultFile)) {
            const auto file = juce::File(getPreferences().getValue(PreferenceKeys::defaultFile));
            openFile(juce::File(getPreferences().getValue(PreferenceKeys::defaultFile)));
        }
    }

    // still nothing loaded? -> create new project
    if (!audiumEngine->getCurrentProjectFile().exists()) {
        audiumEngine->createNewProject();
    }
    
    updateUI();
    
}

void AudiumApplication::shutdown()
{
    getPreferences().setValue(PreferenceKeys::browserWindowOpen, fileBrowserVisible() ? "true" : "false");
    
    // Add your application's shutdown code here..
    mainWindow.reset(); // (deletes our window)

    updateSettings();

#if JUCE_MAC
    MenuBarModel::setMacMainMenu (nullptr);
#endif
    
    menuModel.reset();
    commandManager.reset();
    
    audiumEngine->uninitialise();
    audiumEngine.reset();
}

void AudiumApplication::askToSaveIfDirtyAndInvoke(std::function<void ()> callback)
{
    jassert(callback != nullptr);
    
    if (!audiumEngine->getUndoManager()->canUndo())
    {
        NullCheckedInvocation::invoke(callback);
    }
    else
    {
        juce::String docName = "Untitled";
        if (audiumEngine->getCurrentProjectFile().existsAsFile())
            docName = audiumEngine->getCurrentProjectFile().getFileName();
        

        auto options = MessageBoxOptions::makeOptionsYesNoCancel (MessageBoxIconType::QuestionIcon,
                                                                  TRANS ("Save the changes?"),
                                                                  TRANS ("Do you want to save the changes to \"")
                                                                      + docName + "\"?",
                                                                  TRANS ("Save"),
                                                                  TRANS ("Discard changes"),
                                                                  TRANS ("Cancel"));
        
        // -> std::function<void (int)> callback
        juce::NativeMessageBox::showAsync(options, [this, callback] (int result) {
            if (result == 0) {
                if (audiumEngine->getCurrentProjectFile().existsAsFile()) {
                    if (saveProject())
                        NullCheckedInvocation::invoke(callback);
                }
                else {
                    if (saveProjectAs())
                        NullCheckedInvocation::invoke(callback);
                }
            }
            else if (result == 1) {
                audiumEngine->deleteObsoleteAudioFiles();
                NullCheckedInvocation::invoke(callback);
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
    if (! commandLine.trim().startsWithChar ('-')) {
        ArgumentList list ({}, commandLine);

        for (auto& arg : list.arguments) {
            auto file = arg.resolveAsFile();
            askToSaveIfDirtyAndInvoke([this, file](void) {
                openFile (file);
            
                // will update the open recent menu
                commandManager->commandStatusChanged();
            });
        }
    }
}

void AudiumApplication::initCommandManager()
{
    commandManager = std::make_unique<ApplicationCommandManager>();
    commandManager->registerAllCommandsForTarget (this);
}

void AudiumApplication::initPreferences()
{
    preferences = std::make_unique<Preferences>();
    getPreferences().init(getApplicationName().toStdString(),
                          getApplicationCompanyName().toStdString());
    
    recentFiles.setMaxNumberOfItems(30);
    if (getPreferences().valueExists(PreferenceKeys::recentFiles)) {
        recentFiles.restoreFromString (getPreferences().getValue (PreferenceKeys::recentFiles));
        recentFiles.removeNonExistentFiles();
    }
}

MenuBarModel* AudiumApplication::getMenuModel()
{
    return menuModel.get();
}

StringArray AudiumApplication::getMenuNames()
{
    StringArray currentMenuNames { "File", "Edit", "Create", "View"};
    return currentMenuNames;
}

PopupMenu AudiumApplication::createMenu (const String& menuName)
{
    if (menuName == "File")
        return createFileMenu();

    if (menuName == "Edit")
        return createEditMenu();
    
    if (menuName == "Create")
        return createCreateMenu();

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
    menu.addCommandItem(commandManager.get(), CommandIDs::splitRegion);
    menu.addCommandItem(commandManager.get(), CommandIDs::cleanupRegions);
    menu.addSeparator();
    menu.addCommandItem(commandManager.get(), CommandIDs::autoEdit);
    
#if AUTO_EDIT_ENABLED
    menu.addSeparator();
    menu.addCommandItem(commandManager.get(), CommandIDs::autoEdit);
#endif
    //menu.addSeparator();
    //menu.addCommandItem(commandManager.get(), CommandIDs::loopPlayList);
    return menu;
}

PopupMenu AudiumApplication::createCreateMenu()
{
    PopupMenu menu;
    menu.addCommandItem (commandManager.get(), CommandIDs::createAudioTrack);
    menu.addSeparator();
    menu.addCommandItem(commandManager.get(), CommandIDs::createRegion);
    return menu;
}

PopupMenu AudiumApplication::createViewMenu()
{
    PopupMenu menu;
    menu.addCommandItem (commandManager.get(), CommandIDs::toggleFullScreen);
    menu.addSeparator();
    menu.addCommandItem (commandManager.get(), CommandIDs::showFileBrowser);
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
                                CommandIDs::showFileBrowser,
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
    case CommandIDs::showFileBrowser:
        result.setInfo ("Open File Browser", "Open File Browser window.", CommandCategories::view, 0);
        result.defaultKeypresses.add (KeyPress (KeyPress::numberPad5, ModifierKeys::shiftModifier, 0));
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
            if (audiumEngine->getCurrentProjectFile() != File()) {
                auto projectFile = audiumEngine->getCurrentProjectFile();
                if (!audium::AudiumEngine::isValidProjectStructure(projectFile)) {
                    // projectFile.audium/Project.json -> ../
                    projectFile = projectFile.getParentDirectory();
                }
                jassert(audium::AudiumEngine::isValidProjectStructure(projectFile));
                getPreferences().setValue(PreferenceKeys::defaultFile, projectFile.getFullPathName().toStdString());
            }
            else {
                getPreferences().removeKey(PreferenceKeys::defaultFile);
            }
            break;
        case CommandIDs::saveProject:
            if (audiumEngine->getCurrentProjectFile() == File()) {
                saveProjectAs();
            }
            else {
                saveProject();
            }
            break;
        case CommandIDs::saveProjectAs:
            saveProjectAs();
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
        case CommandIDs::showFileBrowser:
            showFileBrowserWindow();
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
    askToSaveIfDirtyAndInvoke([this](void) {
        audiumEngine->cleanup();
        audiumEngine->createNewProject();
        updateUI();
    });
}

void AudiumApplication::askUserToOpenFile()
{
    askToSaveIfDirtyAndInvoke([this](void) {
    
        // open chooser...
        
        auto wildcard = "*" + String(audium::AudiumEngine::projectFileExtension) + ";";
        wildcard += "*.json;";
        wildcard += audiumEngine->getAudioResourceContainer()->getAudioFormatManager()->getWildcardForAllFormats();
        chooser = std::make_unique<FileChooser> ("Open File", initialOpenDirectory, wildcard);
        auto flags = FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::canSelectDirectories;

        chooser->launchAsync (flags, [this] (const FileChooser& fc) {
            openFile(fc.getResult());
        });
        
    });
}

void AudiumApplication::openFile(juce::File file)
{
    auto success = audiumEngine->openFile(file, [this, file](std::string error) {
        NativeMessageBox::showMessageBoxAsync(MessageBoxIconType::WarningIcon,
                                              "Error",
                                              "Failed to open:\n" + file.getFullPathName() + "\n\n" + String(error));
        
    });
    
    if (success) {
        
        initialOpenDirectory = file.getParentDirectory();
        std::cout << "initialOpenDirectory = " << initialOpenDirectory.getFullPathName() << std::endl;
        RecentlyOpenedFilesList::registerRecentFileNatively (file);
        recentFiles.addFile (file);
        updateSettings();
    }
    
    updateUI();
    
}

const File createProjectDirectory(const File &inFile)
{
    auto projectDir =   inFile.getParentDirectory().getFullPathName() +
                        File::getSeparatorString() +
                        inFile.getFileNameWithoutExtension() +
                        audium::AudiumEngine::projectFileExtension;
    
    if (!File(projectDir).exists()) {
        File(projectDir).createDirectory();
    }
    return File(projectDir + File::getSeparatorString() + audium::AudiumEngine::projectFileName);
}

juce::MessageBoxOptions getAskToOverwriteFileOptions (const File& newFile)
{
    return MessageBoxOptions::makeOptionsOkCancel (MessageBoxIconType::WarningIcon,
                                                   TRANS ("File already exists"),
                                                   TRANS ("There's already a file called: FLNM")
                                                           .replace ("FLNM", newFile.getFullPathName())
                                                       + "\n\n"
                                                       + TRANS ("Are you sure you want to overwrite it?"),
                                                   TRANS ("Overwrite"),
                                                   TRANS ("Cancel"));
}

bool askToOverwriteFileSync (const File& newFile)
{
    auto result = NativeMessageBox::show (getAskToOverwriteFileOptions (newFile));
    return (result == 0);
}


bool AudiumApplication::saveProjectAs()
{
    chooser = std::make_unique<FileChooser> (("Save As..."),
                                             initialSaveDirectory);
    auto flags =    FileBrowserComponent::saveMode |
                    FileBrowserComponent::canSelectFiles |
                    FileBrowserComponent::canSelectDirectories |
                    FileBrowserComponent::warnAboutOverwriting;

    chooser->launchAsync (flags, [this] (const FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file != File()) {
            
            // warn overwrite: special case where user selects an existing project (without the .audium extension)
            auto existingProject = File(file.getFullPathName() + audium::AudiumEngine::projectFileExtension);
            if (existingProject.exists()) {
                if (!askToOverwriteFileSync(existingProject))
                    return false;
            }
            
            if (file.existsAsFile())
                return saveProjectToFile(file);
            else
                return saveProjectToFile(createProjectDirectory(file));
        }
        return false;
    });
    
    return false;
}

bool AudiumApplication::saveProjectToFile(juce::File file)
{
    auto success = audiumEngine->saveFile(file, [this, file] (std::string error) {

        NativeMessageBox::showMessageBoxAsync(MessageBoxIconType::WarningIcon,
                                              "Error",
                                              "Failed to save " + file.getFullPathName() +"\n\n" + String(error));
    });
    
    if (success) {
        // foo.audium/Project.json -> ../../
        initialSaveDirectory = file.getParentDirectory().getParentDirectory();
        std::cout << "file saved: " << file.getFullPathName() << std::endl;
        audiumEngine->deleteObsoleteAudioFiles();
        std::cout << "initialSaveDirectory: " << initialSaveDirectory.getFullPathName() << std::endl;
        
        if (file.getFileName() == audium::AudiumEngine::projectFileName)
            file = file.getParentDirectory();
        
        if (file.getFileName().contains(audium::AudiumEngine::projectFileExtension)) {
            // only save .audium in recent list
            RecentlyOpenedFilesList::registerRecentFileNatively (file);
            recentFiles.addFile (file);
        }
        updateSettings();

    }
    return success;
}

bool AudiumApplication::saveProject()
{
    return saveProjectToFile(audiumEngine->getCurrentProjectFile());
}

void AudiumApplication::updateUI()
{
    if (auto comp = dynamic_cast<MainComponent*>(mainWindow->getContentComponent())) {
        comp->rebuildUI();
        comp->updateUI();
    }
}

void AudiumApplication::showAboutWindow()
{
    auto w = 600;
    auto h = 400;
    if (aboutComponent != nullptr)
        aboutComponent->toFront (true);
    else
        new FloatingToolWindow ("About", "AboutWindowPosition",
                                new AboutWindowComponent(),
                                aboutComponent, false,
                                w, h, w, h, w, h);
}

void AudiumApplication::showFileBrowserWindow()
{
    auto w = 300;
    auto h = 800;
    if (fileBrowserView != nullptr)
        fileBrowserView->toFront (true);
    else
        new FloatingToolWindow ("File Browser", audium::PreferenceKeys::browserWindowState,
                                new FileBrowserView(),
                                fileBrowserView, true,
                                w, h, 50, 50, 3000, 3000);
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
    getPreferences().setValue(PreferenceKeys::initialOpenDirectory, initialOpenDirectory.getFullPathName().toStdString());
    getPreferences().setValue(PreferenceKeys::initialSaveDirectory, initialSaveDirectory.getFullPathName().toStdString());
    getPreferences().setValue(PreferenceKeys::recentFiles, recentFiles.toString().toStdString());
    getPreferences().synchronize();
}

bool AudiumApplication::fileBrowserVisible() const
{
    if (fileBrowserView != nullptr &&
        fileBrowserView->isVisible()) {
        return true;
    }
    return false;
}
