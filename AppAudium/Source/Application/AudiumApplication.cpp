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
#include "Application/AudiumCommandIDs.h"

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
    initCommandManager();

    /// TODO: create factory class
    auto container = std::shared_ptr<AudioResourceContainer>(new AudioResourceContainer());
    audiumEngine.reset(new AudiumEngine(container));
    
    mainWindow.reset (new AudiumMainWindow (getApplicationName(), audiumEngine));
    
}

void AudiumApplication::shutdown()
{
    // Add your application's shutdown code here..

    mainWindow = nullptr; // (deletes our window)
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


    //registerGUIEditorCommands();
}

//==============================================================================
void AudiumApplication::getAllCommands (Array <CommandID>& commands)
{
    JUCEApplication::getAllCommands (commands);

    const CommandID ids[] = { CommandIDs::newProject };

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

    case CommandIDs::open:
        result.setInfo ("Open...", "Opens a project", CommandCategories::general, 0);
        result.defaultKeypresses.add (KeyPress ('o', ModifierKeys::commandModifier, 0));
        break;

//    case CommandIDs::saveAll:
//        result.setInfo ("Save All", "Saves all open documents", CommandCategories::general, 0);
//        result.defaultKeypresses.add (KeyPress ('s', ModifierKeys::commandModifier | ModifierKeys::altModifier, 0));
//        break;

    case CommandIDs::showAboutWindow:
        result.setInfo ("About", "Shows the 'About' page.", CommandCategories::general, 0);
        break;

    case CommandIDs::checkForNewVersion:
        result.setInfo ("Check for New Version...", "Checks the web server for a new version", CommandCategories::general, 0);
        break;

//    case CommandIDs::enableNewVersionCheck:
//        result.setInfo ("Automatically Check for New Versions",
//                        "Enables automatic background checking for new versions of JUCE.",
//                        CommandCategories::general,
//                        (isAutomaticVersionCheckingEnabled() ? ApplicationCommandInfo::isTicked : 0));
//        break;

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
            //createNewProject();
            break;
        case CommandIDs::open:
            //askUserToOpenFile();
            break;
        case CommandIDs::saveDocument:
            //saveAllDocuments();
            break;
        case CommandIDs::showAboutWindow:
            //showAboutWindow();
            break;

        default:
            return JUCEApplication::perform (info);
    }

    return true;
}
