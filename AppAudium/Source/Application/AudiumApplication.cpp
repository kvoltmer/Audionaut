/*
  ==============================================================================

    AudiumApplication.cpp
    Created: 24 Mar 2023 10:48:29am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudiumApplication.h"
#include "Interface/Components/MainComponent.h"
#include "Engine/AudioResourceContainer.h"
#include "Engine/AudiumEngine.h"
#include "Engine/AudiumFactory.h"
#include "Application/AudiumMenuModel.h"
#include "Util/EngineAccess.h"
#include "Util/Preferences.h"

//==============================================================================
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
    Preferences::init(getApplicationName());
    LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    initCommandManager();
    
    // create audium engine
    audiumEngine = AudiumFactory::createAudiumEngine();
    audiumEngine->initialise();

    if (Preferences::valueExists(Preferences::defaultFile))
    {
        //audiumEngine->openFile(juce::File(Preferences::getValue(Preferences::defaultFile)), nullptr);
    }
    
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
    
}

void AudiumApplication::shutdown()
{
    // Add your application's shutdown code here..
    Preferences::synchronize();

#if JUCE_MAC
    MenuBarModel::setMacMainMenu (nullptr);
#endif
    
    menuModel.reset();
    commandManager.reset();
    
    mainWindow = nullptr; // (deletes our window)
    audiumEngine = nullptr;
    
}


void AudiumApplication::systemRequestedQuit()
{
    // This is called when the app is being asked to quit: you can ignore this
    // request and let the app carry on running, or call quit() to allow the app to close.
    quit();
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

PopupMenu AudiumApplication::createFileMenu()
{
    PopupMenu menu;
    menu.addCommandItem (commandManager.get(), CommandIDs::newProject);
    menu.addSeparator();
    menu.addCommandItem (commandManager.get(), CommandIDs::openProject);

    {
        PopupMenu recentFiles;

        //settings->recentFiles.createPopupMenuItems (recentFiles, recentProjectsBaseID, true, true);

        if (recentFiles.getNumItems() > 0)
        {
            recentFiles.addSeparator();
            recentFiles.addCommandItem (commandManager.get(), CommandIDs::clearRecentFiles);
        }

        menu.addSubMenu ("Open Recent", recentFiles);
    }

    menu.addSeparator();
    menu.addCommandItem (commandManager.get(), CommandIDs::saveProject);
    menu.addCommandItem (commandManager.get(), CommandIDs::saveProjectAs);
    menu.addCommandItem (commandManager.get(), CommandIDs::defaultProject);
    menu.addSeparator();

   #if ! JUCE_MAC
    menu.addCommandItem (commandManager.get(), CommandIDs::showAboutWindow);
    menu.addCommandItem (commandManager.get(), CommandIDs::checkForNewVersion);
    menu.addCommandItem (commandManager.get(), CommandIDs::enableNewVersionCheck);
    menu.addCommandItem (commandManager.get(), CommandIDs::showGlobalPathsWindow);
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
    menu.addCommandItem (commandManager.get(), StandardApplicationCommandIDs::del);
    menu.addCommandItem (commandManager.get(), StandardApplicationCommandIDs::selectAll);
    menu.addCommandItem (commandManager.get(), StandardApplicationCommandIDs::deselectAll);
    menu.addSeparator();
//    menu.addCommandItem (commandManager.get(), CommandIDs::showFindPanel);
//    menu.addCommandItem (commandManager.get(), CommandIDs::findSelection);
//    menu.addCommandItem (commandManager.get(), CommandIDs::findNext);
//    menu.addCommandItem (commandManager.get(), CommandIDs::findPrevious);
    menu.addCommandItem(commandManager.get(), CommandIDs::createRegion);
    menu.addCommandItem(commandManager.get(), CommandIDs::autoEdit);
    return menu;
}

PopupMenu AudiumApplication::createViewMenu()
{
    PopupMenu menu;
    menu.addCommandItem (commandManager.get(), CommandIDs::zoomIn);
    menu.addCommandItem (commandManager.get(), CommandIDs::zoomOut);
//    menu.addCommandItem (commandManager.get(), CommandIDs::showExportersPanel);
//    menu.addCommandItem (commandManager.get(), CommandIDs::showExporterSettings);

    return menu;
}

PopupMenu AudiumApplication::createExtraAppleMenuItems()
{
    PopupMenu menu;
    menu.addCommandItem (commandManager.get(), CommandIDs::showAboutWindow);
    return menu;
}


void AudiumApplication::handleMainMenuCommand (int menuItemID)
{
//    if (menuItemID >= recentProjectsBaseID && menuItemID < (recentProjectsBaseID + 100))
//    {
//        // open a file from the "recent files" menu
//        openFile (settings->recentFiles.getFile (menuItemID - recentProjectsBaseID), nullptr);
//    }
}

//==============================================================================
void AudiumApplication::getAllCommands (Array <CommandID>& commands)
{
    JUCEApplication::getAllCommands (commands);

    const CommandID ids[] = { CommandIDs::newProject,
                                CommandIDs::openProject,
                                CommandIDs::defaultProject,
                                CommandIDs::saveProject,
                                CommandIDs::saveProjectAs,
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
        result.setInfo ("Save as Default", "Save this project as default", CommandCategories::general, 0);
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

    case CommandIDs::checkForNewVersion:
        result.setInfo ("Check for New Version...", "Checks the web server for a new version", CommandCategories::general, 0);
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
            audiumEngine->cleanup();
            updateUI();
            break;
        case CommandIDs::openProject:
            askUserToOpenFile();
            break;
        case CommandIDs::defaultProject:
            if (audiumEngine->getCurrentFile() != File{})
            {
                Preferences::setValue(Preferences::defaultFile, audiumEngine->getCurrentFile().getFullPathName());
            }
            else
            {
                Preferences::removeKey(Preferences::defaultFile);
            }
            break;
        case CommandIDs::saveProject:
            if (audiumEngine->getCurrentFile() == File{})
            {
                saveProjectAs();
            }
            else
            {
                saveProject();
            }
            break;
        case CommandIDs::saveProjectAs:
            saveProjectAs();
            break;
        case CommandIDs::showAboutWindow:
            notImplemented();
            //showAboutWindow();
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



void AudiumApplication::askUserToOpenFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Open File", File(), "*" + String(AudiumEngine::projectFileExtension));
    auto flags = FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles;

    chooser->launchAsync (flags, [this] (const FileChooser& fc)
    {
        const auto result = fc.getResult();

        if (result != File{})
        {
            /// TODO: provide and handle callback
            audiumEngine->openFile(result, nullptr);
            updateUI();
        }
    });
}

void AudiumApplication::saveProjectAs()
{
    //chooser = std::make_unique<FileChooser> (("Save As..."), File::SpecialLocationType::userDesktopDirectory, "*");
    chooser = std::make_unique<FileChooser> (("Save As..."), File(), "*" + String(AudiumEngine::projectFileExtension));
    auto flags = FileBrowserComponent::saveMode
               | FileBrowserComponent::canSelectFiles
               | FileBrowserComponent::warnAboutOverwriting;

    chooser->launchAsync (flags, [this] (const FileChooser& fc)
    {
        const auto result = fc.getResult();
        
        if (result != File{})
        {
            /// TODO: provide and handle callback
            audiumEngine->saveFile(result, nullptr);
        }
    });
}

void AudiumApplication::saveProject()
{
    /// TODO: provide and handle callback
    audiumEngine->saveFile(audiumEngine->getCurrentFile(), nullptr);
}

void AudiumApplication::updateUI()
{
    auto comp = dynamic_cast<MainComponent*>(mainWindow->getContentComponent());
    if (comp != nullptr)
    {
        comp->updateUI();
    }
}
