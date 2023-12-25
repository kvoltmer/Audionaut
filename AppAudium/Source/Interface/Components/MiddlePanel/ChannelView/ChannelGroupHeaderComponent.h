/*
  ==============================================================================

    ChannelGroupHeaderComponent.h
    Created: 15 Dec 2023 11:00:09am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "Engine/Group/AudioGroup.h"

class ChannelGroupHeaderComponent  : public juce::Component, public juce::Label::Listener
{
public:
    ChannelGroupHeaderComponent(std::shared_ptr<AudioGroup> audioGroup) :
        audioGroup(audioGroup)
    {
        groupNameLabel.reset (new juce::Label ("group name",
                                             TRANS ("n/a")));
        addAndMakeVisible (groupNameLabel.get());
        groupNameLabel->setFont (juce::Font (12.00f, juce::Font::plain).withTypefaceStyle ("Regular"));
        groupNameLabel->setJustificationType (juce::Justification::centredLeft);
        groupNameLabel->setEditable (false, true, false);
        

        groupNameLabel->setColour (juce::Label::textColourId, audioGroup->getColour());
        groupNameLabel->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        
        groupNameLabel->setColour (juce::Label::outlineColourId, Colours::black);
        
        groupNameLabel->setColour (juce::TextEditor::textColourId, juce::Colours::black);
        
        groupNameLabel->setColour (juce::TextEditor::backgroundColourId, juce::Colour (0x00000000));
        groupNameLabel->setColour (juce::TextEditor::highlightColourId, juce::Colour (0xbff4ff80));
        
        // avoid scaling of text
        groupNameLabel->setMinimumHorizontalScale(1.f);
        
        groupNameLabel->addListener (this);

        groupNameLabel->setBounds (getLocalBounds());
        
        updateFromEngine();
    }

    ~ChannelGroupHeaderComponent() override
    {
    }

    void resized() override
    {
        groupNameLabel->setBounds (getLocalBounds());
    }
    
    void labelTextChanged (juce::Label* labelThatHasChanged) override
    {
        if (labelThatHasChanged == groupNameLabel.get())
        {
            audioGroup->setName(groupNameLabel->getText());
        }
    }
    
    void updateFromEngine()
    {
        groupNameLabel->setText(audioGroup->getName(), dontSendNotification);
    }
    
    void mouseDown (const MouseEvent& event) override
    {
        juce::Component::mouseDown(event);
        
        audioGroup->setSelected(true);
    }

private:
    std::shared_ptr<AudioGroup> audioGroup;
    
    std::unique_ptr<juce::Label> groupNameLabel;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelGroupHeaderComponent)
};
