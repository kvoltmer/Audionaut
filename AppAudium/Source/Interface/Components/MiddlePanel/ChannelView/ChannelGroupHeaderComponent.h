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

class ChannelGroupHeaderComponent : public juce::Component,
                                    public juce::Label::Listener,
                                    public juce::ComboBox::Listener
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
        
        channelSizeComboBox.reset (new juce::ComboBox ("channel size combo box"));
        addAndMakeVisible (channelSizeComboBox.get());
        channelSizeComboBox->setEditableText (false);
        channelSizeComboBox->setJustificationType (juce::Justification::centred);
        channelSizeComboBox->addItem (TRANS ("small"), 1);
        channelSizeComboBox->addItem (TRANS ("medium"), 2);
        channelSizeComboBox->addItem (TRANS ("large"), 3);
        channelSizeComboBox->addItem (TRANS ("huge"), 4);
        channelSizeComboBox->addListener (this);
        // 19 is the dragger height. center vertically -> 19 - 15 = 4 / 2 = 2
        channelSizeComboBox->setBounds (5, 2, 15, 15);
        
        updateFromEngine();
    }

    ~ChannelGroupHeaderComponent() override
    {
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        bounds.removeFromLeft(25);
        groupNameLabel->setBounds(bounds);
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

    void comboBoxChanged (juce::ComboBox* comboBoxThatHasChanged) override
    {
        if (comboBoxThatHasChanged == channelSizeComboBox.get())
        {
            auto height = 0;
            switch (channelSizeComboBox->getSelectedId()) {
                case 1:
                    height = 50;
                    break;
                case 2:
                    height = 100;
                    break;
                case 3:
                    height = 200;
                    break;
                case 4:
                    height = 400;
                    break;
                default:
                    break;
            }

            channelSizeComboBox->setText("", dontSendNotification);

            audioGroup->setChannelHeight(height);
            audioGroup->getAudioResourceContainer().sendActionMessage("");
        }
    }
private:
    std::shared_ptr<AudioGroup> audioGroup;
    
    std::unique_ptr<juce::Label> groupNameLabel;
    std::unique_ptr<juce::ComboBox> channelSizeComboBox;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelGroupHeaderComponent)
};
