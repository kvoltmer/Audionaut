/*
  ==============================================================================

    ChannelGroupHeaderComponent.h
    Created: 15 Dec 2023 11:00:09am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Interface/AudiumLookAndFeel.h"
#include "Interface/Controls/AudiumLabel.h"

class ChannelGroupHeaderComponent : public juce::Component,
                                    public juce::Label::Listener,
                                    public juce::ComboBox::Listener,
                                    public juce::KeyListener
{
public:
    ChannelGroupHeaderComponent(std::shared_ptr<AudioTrack> audioTrack) :
        audioTrack(audioTrack)
    {
        audioTrackNameLabel.reset (new AudiumLabel ("track name",
                                             TRANS ("n/a")));
        addAndMakeVisible (audioTrackNameLabel.get());
        
        audioTrackNameLabel->setFont (juce::FontOptions (12.00f));
        audioTrackNameLabel->setJustificationType (juce::Justification::centredLeft);
        audioTrackNameLabel->setEditable (false, true, false);
    
        // Label colours:
        audioTrackNameLabel->setColour (juce::Label::textColourId, audioTrack->getColour());
        audioTrackNameLabel->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        audioTrackNameLabel->setColour (juce::TextEditor::textColourId, audioTrack->getColour());
        audioTrackNameLabel->setColour (juce::Label::textWhenEditingColourId, audioTrack->getColour());
        audioTrackNameLabel->setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        audioTrackNameLabel->setColour (juce::TextEditor::highlightColourId, juce::Colours::darkgrey);
        
        
        // avoid scaling of text
        audioTrackNameLabel->setMinimumHorizontalScale(1.f);
        
        audioTrackNameLabel->addListener (this);
        
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
        audioTrack = nullptr;
        removeKeyListener(this);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(1, 1);
        bounds.removeFromLeft(25);
        audioTrackNameLabel->setBounds(bounds);
    }
    
    void labelTextChanged (juce::Label* labelThatHasChanged) override
    {
        if (labelThatHasChanged == audioTrackNameLabel.get())
        {
            // undo
            auto action = std::make_unique<audium::UndoableContainerAction>(audioTrack->getAudioTrackContainer(), false);
            
            audioTrack->setAudioTrackName(audioTrackNameLabel->getText());
            
            // undo
            action->storeNewState();
            audioTrack->getAudioTrackContainer().getUndoManager()->perform(action.release(), "Set audio track name");
            audioTrack->getAudioTrackContainer().getUndoManager()->beginNewTransaction();
        }
    }
    
    void updateFromEngine()
    {
        audioTrackNameLabel->setText(audioTrack->getAudioTrackName(), dontSendNotification);
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

            // undo
            auto action = std::make_unique<audium::UndoableContainerAction>(audioTrack->getAudioTrackContainer(), false);
            
            audioTrack->setChannelHeight(height);
            
            // undo
            action->storeNewState();
            audioTrack->getAudioTrackContainer().getUndoManager()->perform(action.release(), "Set audio track height");
            audioTrack->getAudioTrackContainer().getUndoManager()->beginNewTransaction();
        }
    }
    
    
    void paint (juce::Graphics& g) override
    {
        if (audioTrack->isSelected())
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
        return audioTrack->isSelected();
    }
    
    void setSelected(bool bSelected, bool deselectOthers)
    {
        if (deselectOthers)
            audioTrack->getSelectionManager()->deselectAll();
        audioTrack->setSelected(bSelected, false);
        
        audioTrack->getAudioTrackContainer().sendActionMessage(updateMiddlePanelAction);
    }

    bool keyPressed (const KeyPress& key, Component* originatingComponent) override
    {
        if (key.isKeyCode (KeyPress::deleteKey) || key.isKeyCode (KeyPress::backspaceKey))
        {
            
            audioTrack->getAudioTrackContainer().deleteSelectedObjects();
            return true;
        }
        
        return false;
    }
    
private:
    std::shared_ptr<AudioTrack> audioTrack;
    
    std::unique_ptr<AudiumLabel> audioTrackNameLabel;
    std::unique_ptr<juce::ComboBox> channelSizeComboBox;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelGroupHeaderComponent)
};
