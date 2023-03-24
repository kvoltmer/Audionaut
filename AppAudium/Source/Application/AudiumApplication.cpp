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

//==============================================================================
void AudiumApplication::initialise (const juce::String& commandLine)
{
    // This method is where you should put your application's initialisation code..

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

//==============================================================================
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
