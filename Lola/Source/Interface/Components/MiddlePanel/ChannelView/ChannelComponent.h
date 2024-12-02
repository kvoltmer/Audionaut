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

#include "Engine/Resource/AudioResource.h"
#include "Engine/AudiumEngine.h"

#include "Interface/Controls/LevelMeter.h"

//[/Headers]



//==============================================================================
/**
                                                                    //[Comments]
    An auto-generated component, created by the Projucer.

    Describe your class and how it works here!
                                                                    //[/Comments]
*/
class ChannelComponent  : public juce::Component,
                          private juce::Timer,
                          public juce::ComboBox::Listener,
                          public juce::Button::Listener,
                          public juce::Label::Listener
{
public:
    //==============================================================================
    ChannelComponent (std::shared_ptr<AudioTrack> audioTrack, std::shared_ptr<AudiumEngine> engine, int rowNumber);
    ~ChannelComponent() override;

    //==============================================================================
    //[UserMethods]     -- You can add your own custom methods in this section.
    void refreshComponent(std::shared_ptr<AudioTrack> audioTrack, int rowNumber, bool isRowSelected);
    void timerCallback() override;
    void stopTheTimer() { stopTimer(); }
    void comboBoxChanged (juce::ComboBox* comboBoxThatHasChanged) override;
    //[/UserMethods]

    void paint (juce::Graphics& g) override;
    void resized() override;
    void buttonClicked (juce::Button* buttonThatWasClicked) override;
    void labelTextChanged (juce::Label* labelThatHasChanged) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    bool keyPressed (const juce::KeyPress& key) override;

    // Binary resources:
    static const char* channelScale_png;
    static const int channelScale_pngSize;

    enum { moveChannelToNewTrackId = 0xf836743, reservedId = 0xf836744 };

    std::shared_ptr<AudiumEngine> getEngine() const { return engine; }
    
private:
    //[UserVariables]   -- You can add your own custom variables in this section.
    std::shared_ptr<AudioTrack> audioTrack;
    std::shared_ptr<AudiumEngine> engine;
    std::unique_ptr<LevelMeter> levelMeter;
    std::unique_ptr<juce::ComboBox> channelSizeComboBox;
    int rowNumber = 0;
    //[/UserVariables]

    //==============================================================================
    std::unique_ptr<juce::Label> volumeLabeldB;
    std::unique_ptr<juce::ImageButton> volumeScaleButton;
    std::unique_ptr<juce::Label> volumeLevel;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelComponent)
};

//[EndFile] You can add extra defines here...
//[/EndFile]

