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

AudiumMainWindow::AudiumMainWindow (juce::String name, std::shared_ptr<AudiumEngine> audiumEngine)
    : DocumentWindow (name,
                      juce::Desktop::getInstance().getDefaultLookAndFeel()
                                                  .findColour (juce::ResizableWindow::backgroundColourId),
                      DocumentWindow::allButtons),
    audiumEngine(audiumEngine)
{
    setUsingNativeTitleBar (true);
    setContentOwned (new MainComponent(audiumEngine), true);

    
    auto& commandManager = AudiumApplication::getCommandManager();

    auto registerAllAppCommands = [&]
    {
        commandManager.registerAllCommandsForTarget (this);
        //commandManager.registerAllCommandsForTarget (getProjectContentComponent());
    };

    auto updateAppKeyMappings = [&]
    {
        commandManager.getKeyMappings()->resetToDefaultMappings();

//        if (auto keys = getGlobalProperties().getXmlValue ("keyMappings"))
//            commandManager.getKeyMappings()->restoreFromXml (*keys);

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
        CommandIDs::playStop
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


        default:
            break;
    }
}

bool AudiumMainWindow::perform (const InvocationInfo& info)
{
    switch (info.commandID)
    {
        case CommandIDs::playStop:
            getEngine()->getAudioResourceContainer()->playStop();
            break;

        default:
            return false;
    }

    return true;
}
