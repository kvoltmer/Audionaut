/*
  ==============================================================================

    AudiumMainWindow.cpp
    Created: 24 Mar 2023 10:51:48am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudiumMainWindow.h"
#include "AudiumApplication.h"
#include "AudiumCommandIDs.h"
#include "Util/EngineAccess.h"
#include "Engine/TransportSourceProvider.h"
#include "Engine/PlayList/PlayListScheduler.h"


AudiumMainWindow::AudiumMainWindow (juce::String name, std::shared_ptr<AudiumEngine> audiumEngine)
    : DocumentWindow (name,
                      juce::Desktop::getInstance().getDefaultLookAndFeel()
                                                  .findColour (juce::ResizableWindow::backgroundColourId),
                      DocumentWindow::allButtons),
    audiumEngine(audiumEngine)
{
    setUsingNativeTitleBar (true);
    mainComponent.reset(new MainComponent(audiumEngine));
    setContentOwned (mainComponent.get(), true);
    
#if ! JUCE_MAC
 setMenuBar (ProjucerApplication::getApp().getMenuModel());
#endif
    
    auto& commandManager = AudiumApplication::getCommandManager();

    auto registerAllAppCommands = [&]
    {
        commandManager.registerAllCommandsForTarget (this);
    };

    auto updateAppKeyMappings = [&]
    {
        commandManager.getKeyMappings()->resetToDefaultMappings();

        addKeyListener (commandManager.getKeyMappings());
    };

    
    registerAllAppCommands();
    updateAppKeyMappings();
    
    
   #if JUCE_IOS || JUCE_ANDROID
    setFullScreen (true);
   #else
    setResizable (true, true);
    centreWithSize (getWidth(), getHeight());
   #endif

    setVisible (true);
}

AudiumMainWindow::~AudiumMainWindow()
{
#if ! JUCE_MAC
    setMenuBar (nullptr);
#endif
}

void AudiumMainWindow::closeButtonPressed()
{
    // This is called when the user tries to close this window. Here, we'll just
    // ask the app to quit when this happens, but you can change this to do
    // whatever you need.
    JUCEApplication::getInstance()->systemRequestedQuit();
}

//==============================================================================
ApplicationCommandTarget* AudiumMainWindow::getNextCommandTarget()
{
    return nullptr;
}

void AudiumMainWindow::getAllCommands (Array <CommandID>& commands)
{
    const CommandID ids[] =
    {
        CommandIDs::playStop,
        CommandIDs::createRegion,
        CommandIDs::autoEdit,
        CommandIDs::zoomIn,
        CommandIDs::zoomOut
    };

    commands.addArray (ids, numElementsInArray (ids));
}

void AudiumMainWindow::getCommandInfo (const CommandID commandID, ApplicationCommandInfo& result)
{
    switch (commandID)
    {
        case CommandIDs::playStop:
            result.setInfo ("Play/Stop", "Play and stop", CommandCategories::transport, 0);
            result.defaultKeypresses.add (KeyPress (' ', ModifierKeys::noModifiers, 0));
            break;
        case CommandIDs::createRegion:
            result.setInfo ("Create Region", "Creates a new region", CommandCategories::editing, 0);
            result.defaultKeypresses.add (KeyPress ('r', ModifierKeys::commandModifier, 0));
            break;
        case CommandIDs::autoEdit:
            result.setInfo ("Auto Edit", "Automatically creates an Edit", CommandCategories::editing, 0);
            result.defaultKeypresses.add (KeyPress ('e', ModifierKeys::commandModifier, 0));
            break;
        case CommandIDs::zoomIn:
            result.setInfo ("Zoom In", "Zoom in", CommandCategories::view, 0);
            result.defaultKeypresses.add (KeyPress ('t', ModifierKeys::ctrlModifier, 0));
            break;
        case CommandIDs::zoomOut:
            result.setInfo ("Zoom Out", "Zoom out", CommandCategories::view, 0);
            result.defaultKeypresses.add (KeyPress ('r', ModifierKeys::ctrlModifier, 0));
            break;
        default:
            break;
    }
}

bool AudiumMainWindow::perform (const InvocationInfo& info)
{
    switch (info.commandID)
    {
        case CommandIDs::playStop:
            if (getEngine()->getTransportSourceProvider()->playStop() == false)
            {
                getEngine()->getPlayListScheduler()->stop();
            }
            mainComponent->updateUI();
            break;
        case CommandIDs::createRegion:
            newRegionDialog.createNewRegion(getEngine());
            break;
        case CommandIDs::autoEdit:
            autoEditDialog.invokeAutoEdit(getEngine(), mainComponent);
            break;
        case CommandIDs::zoomIn:
            mainComponent->zoomIn();
            break;
        case CommandIDs::zoomOut:
            mainComponent->zoomOut();
            break;

        default:
            return false;
    }

    return true;
}
