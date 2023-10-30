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
class PlayListScheduler;
//[/Headers]



//==============================================================================
/**
                                                                    //[Comments]
    An auto-generated component, created by the Projucer.

    Describe your class and how it works here!
                                                                    //[/Comments]
*/
class HeaderComponent  : public juce::Component,
                         private juce::Timer,
                         public juce::Button::Listener,
                         public juce::Label::Listener
{
public:
    //==============================================================================
    HeaderComponent (std::shared_ptr<PlayListScheduler> playListScheduler);
    ~HeaderComponent() override;

    //==============================================================================
    //[UserMethods]     -- You can add your own custom methods in this section.
    void timerCallback() override;
    //[/UserMethods]

    void paint (juce::Graphics& g) override;
    void resized() override;
    void buttonClicked (juce::Button* buttonThatWasClicked) override;
    void labelTextChanged (juce::Label* labelThatHasChanged) override;



private:
    //[UserVariables]   -- You can add your own custom variables in this section.
    std::shared_ptr<PlayListScheduler> playListScheduler;
    //[/UserVariables]

    //==============================================================================
    std::unique_ptr<juce::TextButton> link__textButton;
    std::unique_ptr<juce::Label> tempo__label;
    std::unique_ptr<juce::Label> bars__label;
    std::unique_ptr<juce::Label> beats__label;
    std::unique_ptr<juce::Label> rest__label;


    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeaderComponent)
};

//[EndFile] You can add extra defines here...
//[/EndFile]

