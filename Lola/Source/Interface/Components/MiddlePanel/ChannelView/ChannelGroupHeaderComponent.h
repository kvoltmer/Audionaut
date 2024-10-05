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
#include "Engine/Group/AudioGroupContainer.h"
#include "Interface/AudiumLookAndFeel.h"
#include "Interface/Controls/AudiumLabel.h"

class ChannelGroupHeaderComponent : public juce::Component,
                                    public juce::Label::Listener,
                                    public juce::ComboBox::Listener,
                                    public juce::KeyListener
{
public:
    ChannelGroupHeaderComponent(std::shared_ptr<AudioGroup> audioGroup) :
        audioGroup(audioGroup)
    {
        groupNameLabel.reset (new AudiumLabel ("group name",
                                             TRANS ("n/a")));
        addAndMakeVisible (groupNameLabel.get());
        groupNameLabel->setFont (juce::Font (12.00f, juce::Font::plain).withTypefaceStyle ("Regular"));
        groupNameLabel->setJustificationType (juce::Justification::centredLeft);
        groupNameLabel->setEditable (false, true, false);
        

        groupNameLabel->setColour (juce::Label::textColourId, audioGroup->getColour());
        groupNameLabel->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        //groupNameLabel->setColour (juce::Label::outlineColourId, Colours::black);
        groupNameLabel->setColour (juce::TextEditor::textColourId, juce::Colours::black);
        groupNameLabel->setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
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
        
        addKeyListener(this);
    }

    ~ChannelGroupHeaderComponent() override
    {
        audioGroup = nullptr;
        removeKeyListener(this);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(1, 1);
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
    
    
    void paint (juce::Graphics& g) override
    {
        if (audioGroup->isSelected())
        {
            auto colour = findColour(audium::secondaryBackgroundColourId).brighter().withAlpha(0.3f);
            g.fillAll (colour);
        }
        
        g.setColour (Colours::black);
        g.drawRect (getLocalBounds(), 1);   // draw an outline around the component
    }
    
    void mouseDown (const juce::MouseEvent& e) override
    {
        setSelected(e.mods.isCommandDown() ? !isSelected() : true, !e.mods.isCommandDown());
    }
    
    bool isSelected() const
    {
        return audioGroup->isSelected();
    }
    
    void setSelected(bool bSelected, bool deselectOthers)
    {
        if (deselectOthers)
            audioGroup->getAudioGroupContainer().selectAllGroups(false, true);
        audioGroup->setSelected(bSelected, false);
        
        audioGroup->getAudioGroupContainer().sendActionMessage(updateMiddlePanelAction);
    }

    bool keyPressed (const KeyPress& key, Component* originatingComponent) override
    {
        if (key.isKeyCode (KeyPress::deleteKey) || key.isKeyCode (KeyPress::backspaceKey))
        {
            
            audioGroup->getAudioGroupContainer().deleteSelectedObjects();
            return true;
        }
        
        return false;
    }
    
private:
    std::shared_ptr<AudioGroup> audioGroup;
    
    std::unique_ptr<AudiumLabel> groupNameLabel;
    std::unique_ptr<juce::ComboBox> channelSizeComboBox;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelGroupHeaderComponent)
};
