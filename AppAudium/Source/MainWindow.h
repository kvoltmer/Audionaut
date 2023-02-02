/*
  ==============================================================================

    MainWindow.h
    Created: 30 Jan 2023 11:08:01am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Engine/AudiumEngine.h"
#include "Interface/Components/MainComponent.h"

//==============================================================================
/*
    This class implements the desktop window that contains an instance of
    our MainComponent class.
*/
class MainWindow    : public juce::DocumentWindow
{
public:
    MainWindow (juce::String name, std::shared_ptr<AudiumEngine> audiumEngine)
        : DocumentWindow (name,
                          juce::Desktop::getInstance().getDefaultLookAndFeel()
                                                      .findColour (juce::ResizableWindow::backgroundColourId),
                          DocumentWindow::allButtons),
        audiumEngine(audiumEngine)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (new MainComponent(), true);

       #if JUCE_IOS || JUCE_ANDROID
        setFullScreen (true);
       #else
        setResizable (true, true);
        centreWithSize (getWidth(), getHeight());
       #endif

        setVisible (true);
    }

    void closeButtonPressed() override
    {
        // This is called when the user tries to close this window. Here, we'll just
        // ask the app to quit when this happens, but you can change this to do
        // whatever you need.
        JUCEApplication::getInstance()->systemRequestedQuit();
    }

    /* Note: Be careful if you override any DocumentWindow methods - the base
       class uses a lot of them, so by overriding you might break its functionality.
       It's best to do all your work in your content component instead, but if
       you really have to override any DocumentWindow methods, make sure your
       subclass also calls the superclass's method.
    */
    
    std::shared_ptr<AudiumEngine> getEngine() const { return audiumEngine; }
    
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    std::shared_ptr<AudiumEngine> audiumEngine;
};
