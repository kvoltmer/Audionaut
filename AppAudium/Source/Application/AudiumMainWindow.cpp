/*
  ==============================================================================

    AudiumMainWindow.cpp
    Created: 24 Mar 2023 10:51:48am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "AudiumMainWindow.h"

AudiumMainWindow::AudiumMainWindow (juce::String name, std::shared_ptr<AudiumEngine> audiumEngine)
    : DocumentWindow (name,
                      juce::Desktop::getInstance().getDefaultLookAndFeel()
                                                  .findColour (juce::ResizableWindow::backgroundColourId),
                      DocumentWindow::allButtons),
    audiumEngine(audiumEngine)
{
    setUsingNativeTitleBar (true);
    setContentOwned (new MainComponent(audiumEngine), true);

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
