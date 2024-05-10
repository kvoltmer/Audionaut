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
//[/Headers]



//==============================================================================
/**
                                                                    //[Comments]
    An auto-generated component, created by the Projucer.

    Describe your class and how it works here!
                                                                    //[/Comments]
*/
class AutoEditComponent  : public juce::Component,
                           public juce::ComboBox::Listener
{
public:
    //==============================================================================
    AutoEditComponent ();
    ~AutoEditComponent() override;

    //==============================================================================
    //[UserMethods]     -- You can add your own custom methods in this section.
    juce::Value& getDuration() const { return duration->getTextValue(); }
    juce::Value& getNumSegments() const { return numSegments->getTextValue(); }
    juce::Value& getMinSegmentLength() const { return segmentMin->getTextValue(); }
    juce::Value& getMaxSegmentLength() const { return segmentMax->getTextValue(); }
    juce::Value& getEditMode() const { return mode->getSelectedIdAsValue(); }
    //[/UserMethods]

    void paint (juce::Graphics& g) override;
    void resized() override;
    void comboBoxChanged (juce::ComboBox* comboBoxThatHasChanged) override;



private:
    //[UserVariables]   -- You can add your own custom variables in this section.
    //[/UserVariables]

    //==============================================================================
    std::unique_ptr<juce::Label> juce__label;
    std::unique_ptr<juce::Label> juce__label2;
    std::unique_ptr<juce::Label> juce__label3;
    std::unique_ptr<juce::Label> juce__label4;
    std::unique_ptr<juce::TextEditor> duration;
    std::unique_ptr<juce::TextEditor> numSegments;
    std::unique_ptr<juce::TextEditor> segmentMin;
    std::unique_ptr<juce::TextEditor> segmentMax;
    std::unique_ptr<juce::ComboBox> mode;
    std::unique_ptr<juce::Label> juce__label5;


    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoEditComponent)
};

//[EndFile] You can add extra defines here...
//[/EndFile]

