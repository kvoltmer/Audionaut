//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <JuceHeader.h>

#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/ActionMessages.h"

#include "Interface/LookAndFeel/AudiumLookAndFeel.h"
#include "Interface/Controls/AudiumLabel.h"

class ChannelGroupHeaderComponent : public juce::Component,
                                    public juce::Label::Listener,
                                    public juce::ComboBox::Listener,
                                    public juce::KeyListener,
                                    public juce::DragAndDropTarget
{
public:
    ChannelGroupHeaderComponent(std::shared_ptr<audium::AudioTrack> audioTrack) :
        audioTrack(audioTrack)
    {
        audioTrackNameLabel.reset (new AudiumLabel ("track name",
                                             TRANS ("n/a")));
        addAndMakeVisible (audioTrackNameLabel.get());
        
        audioTrackNameLabel->setFont (juce::FontOptions (12.00f));
        audioTrackNameLabel->setJustificationType (juce::Justification::centredLeft);
        audioTrackNameLabel->setEditable (false, true, false);
    
        // Label colours (also see updateFromEngine):
        audioTrackNameLabel->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
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
        
        updateFromEngine(audioTrack);
        
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
    
    void updateFromEngine(std::shared_ptr<audium::AudioTrack> newAudioTrack)
    {
        audioTrack = newAudioTrack;
        audioTrackNameLabel->setText(audioTrack->getAudioTrackName(), juce::dontSendNotification);
        audioTrackNameLabel->setColour (juce::Label::textColourId, audioTrack->getColour());
        audioTrackNameLabel->setColour (juce::TextEditor::textColourId, audioTrack->getColour());
        audioTrackNameLabel->setColour (juce::Label::textWhenEditingColourId, audioTrack->getColour());
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

            channelSizeComboBox->setText("", juce::dontSendNotification);

            // undo
            auto action = std::make_unique<audium::UndoableContainerAction>(audioTrack->getAudioTrackContainer(), false);
            
            audioTrack->setChannelHeight(height);
            
            // undo
            action->storeNewState();
            audioTrack->getAudioTrackContainer().getUndoManager()->perform(action.release(), "Set audio track height");
            audioTrack->getAudioTrackContainer().getUndoManager()->beginNewTransaction();
        }
    }
    
    
    void paint (juce::Graphics& g) override;
    
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
        
        audioTrack->getAudioTrackContainer().sendActionMessage(audium::updateMiddlePanelAction);
    }

    bool keyPressed (const juce::KeyPress& key, juce::Component* originatingComponent) override
    {
        if (key.isKeyCode (juce::KeyPress::deleteKey) || key.isKeyCode (juce::KeyPress::backspaceKey)) {
            audioTrack->getAudioTrackContainer().deleteSelectedObjects();
            return true;
        }
        return false;
    }
    
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (juce::DragAndDropContainer* container = juce::DragAndDropContainer::findParentDragContainerFor(this)) {
            container->startDragging("ChannelGroupHeaderComponent", this);
        }
    }
        
    bool isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails &dragSourceDetails) override;
    
    void updateInsertLines(const juce::DragAndDropTarget::SourceDetails &dragSourceDetails);
    
    void hideInsertLines()
    {
        insertBefore = false;
        
        repaint();
    }
    void itemDragEnter (const SourceDetails &dragSourceDetails) override
    {
        updateInsertLines(dragSourceDetails);
    }
    
    void itemDragMove (const SourceDetails &dragSourceDetails) override
    {
        updateInsertLines(dragSourceDetails);
    }
    
    void itemDragExit (const SourceDetails &dragSourceDetails) override
    {
        hideInsertLines();
    }
    
    void itemDropped (const SourceDetails &dragSourceDetails) override;
    
    bool shouldDrawDragImageWhenOver () override
    {
        return true;
    }
    std::shared_ptr<audium::AudioTrack> getAudioTrack() const { return audioTrack; }

private:
    std::shared_ptr<audium::AudioTrack> audioTrack;
    
    std::unique_ptr<AudiumLabel> audioTrackNameLabel;
    std::unique_ptr<juce::ComboBox> channelSizeComboBox;
    
    bool insertBefore = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelGroupHeaderComponent)
};
