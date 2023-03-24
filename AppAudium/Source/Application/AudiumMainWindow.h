/*
  ==============================================================================

    AudiumMainWindow.h
    Created: 24 Mar 2023 10:51:48am
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
class AudiumMainWindow    : public juce::DocumentWindow
{
public:
    AudiumMainWindow (juce::String name, std::shared_ptr<AudiumEngine> audiumEngine);

    void closeButtonPressed() override;

    /* Note: Be careful if you override any DocumentWindow methods - the base
       class uses a lot of them, so by overriding you might break its functionality.
       It's best to do all your work in your content component instead, but if
       you really have to override any DocumentWindow methods, make sure your
       subclass also calls the superclass's method.
    */
    
    std::shared_ptr<AudiumEngine> getEngine() const { return audiumEngine; }
    
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudiumMainWindow)
    std::shared_ptr<AudiumEngine> audiumEngine;
};
