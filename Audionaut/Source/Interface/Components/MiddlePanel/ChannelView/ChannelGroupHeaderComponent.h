//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/ActionMessages.h"

#include "Interface/LookAndFeel/AudiumLookAndFeel.h"
#include "Interface/Controls/AudiumLabel.h"

#include "Application/AudiumCommandIDs.h"

class ChannelGroupHeaderComponent : public juce::Component,
                                    public juce::Label::Listener,
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
        audioTrackNameLabel->setMinimumHorizontalScale(1.f); // avoid scaling of text
        audioTrackNameLabel->addListener (this);
        
        
        minimizeButton = std::make_unique<juce::DrawableButton>("Minimize", juce::DrawableButton::ButtonStyle::ImageOnButtonBackground);
        addAndMakeVisible(minimizeButton.get());
        minimizeButton->setClickingTogglesState(true);
        
        auto arrowSize = 15.f;
        juce::Rectangle<float> arrowZone (1.f, 1.f, arrowSize - 2.f, arrowSize - 2.f);
        Point<float> p;
        
        Path path1;
        p = Point(arrowZone.getX() + 3.f, arrowZone.getCentreY() - 2.f);
        path1.startNewSubPath (p);
        p = Point(arrowZone.getCentreX(), arrowZone.getCentreY() + 3.f);
        path1.lineTo (p);
        p = Point(arrowZone.getRight() - 3.f, arrowZone.getCentreY() - 2.f);
        path1.lineTo (p);
        juce::DrawablePath drawablePath1;
        drawablePath1.setPath(path1);
        drawablePath1.setFill (FillType(findColour (ComboBox::arrowColourId).withAlpha (0.9f)));
        
        Path path2;
        p = Point(arrowZone.getCentreX() - 2.f, arrowZone.getCentreY() + 3.f);
        path2.startNewSubPath (p);
        p = Point(arrowZone.getRight() - 3.f, arrowZone.getCentreY());
        path2.lineTo (p);
        p = Point(arrowZone.getCentreX() - 2.f, arrowZone.getCentreY() - 3.f);
        path2.lineTo (p);
        juce::DrawablePath drawablePath2;
        drawablePath2.setPath(path2);
        drawablePath2.setFill (FillType(findColour (ComboBox::arrowColourId).withAlpha (0.9f)));
        
        minimizeButton->setImages(&drawablePath1, nullptr, nullptr, nullptr, &drawablePath2);
        
        auto bc = findColour (ComboBox::backgroundColourId);
        minimizeButton->setColour (TextButton::buttonOnColourId, bc);
        minimizeButton->setColour (TextButton::buttonColourId, bc);
        
        // 19 is the dragger height. center horizontally -> 19 - 15 = 4 / 2 = 2
        minimizeButton->setBounds (5, 2, 15, 15);
        
        minimizeButton->onClick = [this]() {
            setMinimized (minimizeButton->getToggleState());
        };
   
        updateFromEngine (audioTrack);
        
        addKeyListener (this);
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
        audioTrackNameLabel->setText (audioTrack->getAudioTrackName(), juce::dontSendNotification);
        audioTrackNameLabel->setColour (juce::Label::textColourId, audioTrack->getColour());
        audioTrackNameLabel->setColour (juce::TextEditor::textColourId, audioTrack->getColour());
        audioTrackNameLabel->setColour (juce::Label::textWhenEditingColourId, audioTrack->getColour());
        
        minimizeButton->setToggleState (audioTrack->getMinimized(), dontSendNotification);
    }
    
    void paint (juce::Graphics& g) override;
    
    static void contextMenuCallback (int result, ChannelGroupHeaderComponent* component)
    {
        if (component != nullptr &&
            result != 0) {
            
            auto height = result;
            component->setChannelHeight(height);
        }
    }
    
    void setMinimized(bool bMinimized)
    {
        // undo
        auto action = std::make_unique<audium::UndoableContainerAction>(audioTrack->getAudioTrackContainer(), false);
        
        audioTrack->setMinimized(bMinimized);
        
        // undo
        action->storeNewState();
        audioTrack->getAudioTrackContainer().getUndoManager()->perform(action.release(), "minimize audio track");
        audioTrack->getAudioTrackContainer().getUndoManager()->beginNewTransaction();
    }
    
    void setChannelHeight(int height)
    {
        // undo
        auto action = std::make_unique<audium::UndoableContainerAction>(audioTrack->getAudioTrackContainer(), false);
        
        jassert(height > 0);
        audioTrack->setMinimized(false);
        audioTrack->setChannelHeight(height);
        
        // undo
        action->storeNewState();
        audioTrack->getAudioTrackContainer().getUndoManager()->perform(action.release(), "Set audio track height");
        audioTrack->getAudioTrackContainer().getUndoManager()->beginNewTransaction();
    }
    
    
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) {
            PopupMenu m;
            m.addItem (AudiumLookAndFeel::SizeIds::micro, TRANS ("micro"), true);
            m.addItem (AudiumLookAndFeel::SizeIds::small, TRANS ("small"), true);
            m.addItem (AudiumLookAndFeel::SizeIds::medium, TRANS ("medium"), true);
            m.addItem (AudiumLookAndFeel::SizeIds::large, TRANS ("large"), true);
            m.addItem (AudiumLookAndFeel::SizeIds::huge, TRANS ("huge"), true);
            m.setLookAndFeel (&getLookAndFeel());
            m.showMenuAsync (PopupMenu::Options().withStandardItemHeight(AudiumLookAndFeel::popupMenuItemHeight),
                             ModalCallbackFunction::forComponent (contextMenuCallback, this));
        }
        
        if (!e.mods.isAnyModifierKeyDown() && !isSelected()) {
            audioTrack->getSelectionManager()->deselectAll();
        }
        
        setSelected(e.mods.isCommandDown() ? !isSelected() : true);
    }
    
    bool isSelected() const
    {
        return audioTrack->isSelected();
    }
    
    void setSelected(bool bSelected)
    {
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
    std::unique_ptr<juce::DrawableButton> minimizeButton;
    
    bool insertBefore = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelGroupHeaderComponent)
};
