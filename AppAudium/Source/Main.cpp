/*
  ==============================================================================

    This file contains the basic startup code for a JUCE application.

  ==============================================================================
*/

#include <JuceHeader.h>
#include "MainWindow.h"
#include "Interface/Components/MainComponent.h"
#include "Engine/AudioResourceContainer.h"
#include "Engine/AudiumEngine.h"

class AudiumEngine;

//==============================================================================
class AppAudiumApplication  : public juce::JUCEApplication
{
public:
    //==============================================================================
    AppAudiumApplication() {}

    const juce::String getApplicationName() override       { return ProjectInfo::projectName; }
    const juce::String getApplicationVersion() override    { return ProjectInfo::versionString; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    //==============================================================================
    void initialise (const juce::String& commandLine) override
    {
        // This method is where you should put your application's initialisation code..

        /// TODO: create factory class
        auto container = std::shared_ptr<AudioResourceContainer>(new AudioResourceContainer());
        audiumEngine.reset(new AudiumEngine(container));
        
        mainWindow.reset (new MainWindow (getApplicationName(), audiumEngine));
    }

    void shutdown() override
    {
        // Add your application's shutdown code here..

        mainWindow = nullptr; // (deletes our window)
    }

    //==============================================================================
    void systemRequestedQuit() override
    {
        // This is called when the app is being asked to quit: you can ignore this
        // request and let the app carry on running, or call quit() to allow the app to close.
        quit();
    }

    void anotherInstanceStarted (const juce::String& commandLine) override
    {
        // When another instance of the app is launched while this one is running,
        // this method is invoked, and the commandLine parameter tells you what
        // the other instance's command-line arguments were.
    }



private:
    std::unique_ptr<MainWindow> mainWindow;
    std::shared_ptr<AudiumEngine> audiumEngine;
};

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION (AppAudiumApplication)
