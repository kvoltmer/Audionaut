/*
  ==============================================================================

  This is an automatically generated GUI class created by the Projucer!

  Be careful when adding custom code to these files, as only the code within
  the "//[xyz]" and "//[/xyz]" sections will be retained when the file is loaded
  and re-saved.

  Created with Projucer version: 7.0.8

  ------------------------------------------------------------------------------

  The Projucer is part of the JUCE library.
  Copyright (c) 2020 - Raw Material Software Limited.

  ==============================================================================
*/

#pragma once

//[Headers]     -- You can add your own extra header files here --
#include <JuceHeader.h>


using namespace juce;

class AudiumEngine;
class MiddlePanelComponent;
class RightPanelComponent;
class HeaderComponent;

//[/Headers]



//==============================================================================
/**
                                                                    //[Comments]
    An auto-generated component, created by the Projucer.

    Describe your class and how it works here!
                                                                    //[/Comments]
*/
class MainComponent  : public juce::Component,
                       private juce::ActionListener
{
public:
    //==============================================================================
    MainComponent (std::shared_ptr<AudiumEngine> audiumEngine);
    ~MainComponent() override;

    //==============================================================================
    //[UserMethods]     -- You can add your own custom methods in this section.
    void actionListenerCallback (const String& message) override;
    void updateUI();
    void rebuildUI();

    void zoomIn();
    void zoomOut();

    void pageLeft();
    void pageRight();

    void toggleEditArrangementComponent();
    //[/UserMethods]

    void paint (juce::Graphics& g) override;
    void resized() override;



private:
    //[UserVariables]   -- You can add your own custom variables in this section.

    std::shared_ptr<AudiumEngine> audiumEngine;

    std::unique_ptr<HeaderComponent> headerComponent;
    std::unique_ptr<MiddlePanelComponent> middlePanelComponent;
    std::unique_ptr<RightPanelComponent> rightPanelComponent;

    std::unique_ptr<StretchableLayoutManager> stretchableLayoutManager;
    std::unique_ptr<StretchableLayoutResizerBar> stretchableLayoutResizerBar;

    //[/UserVariables]

    //==============================================================================


    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

//[EndFile] You can add extra defines here...
//[/EndFile]

