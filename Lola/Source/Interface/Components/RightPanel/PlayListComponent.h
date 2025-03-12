/*
  ==============================================================================

    PlayListComponent.h
    Created: 27 Jun 2023 2:05:00pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Engine/AudiumEngine.h"
#include "Engine/Group/AudioTrack.h"

#include "Interface/Models/PlayListTableListBoxModel.h"
#include "Interface/Controls/PlayListTableListBox.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"

//==============================================================================
/*
*/
class PlayListComponent  : public juce::Component, public juce::DragAndDropTarget, public juce::AsyncUpdater
{
public:
    PlayListComponent(std::shared_ptr<AudiumEngine> audiumEngine, std::shared_ptr<AudioTrack> track) :
        audiumEngine(audiumEngine),
        audioTrack(track)
    {
        playListTableListBox.reset(new PlayListTableListBox(this));

        playListTableListBox->setMultipleSelectionEnabled(true);
        addAndMakeVisible(playListTableListBox.get());
        
        playListTableListBox->getHeader().addColumn ("", 1, 250, 0, 800,
                                                     juce::TableHeaderComponent::notResizableOrSortable);
        playListTableListBox->getHeader().setStretchToFitActive (true);
        
        playListTableListBox->setHeaderHeight(AudiumLookAndFeel::tableHeaderHeight);
        playListTableListBox->setOutlineThickness (0);
        playListTableListBox->setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        
        playListTableListBoxModel = std::make_unique<PlayListTableListBoxModel> (playListTableListBox, audiumEngine, track);
        playListTableListBox->setModel(playListTableListBoxModel.get());
        updateUI(track);
    }

    ~PlayListComponent() override
    {
        playListTableListBox->setModel(nullptr);
        playListTableListBox = nullptr;
        playListTableListBoxModel = nullptr;
    }

    void paint (juce::Graphics& g) override;

    void resized() override
    {
        playListTableListBox->setBounds(getLocalBounds());
    }
    
    void handleAsyncUpdate() override
    {
        playListTableListBox->updateContent();
    }
    
    void updateInsertLines(const SourceDetails &dragSourceDetails);

    bool isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails &dragSourceDetails) override;
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
        itemDrag = false;
        repaint();
    }
    
    void itemDropped (const SourceDetails &dragSourceDetails) override;
    
    bool shouldDrawDragImageWhenOver () override
    {
        return true;
    }
    
    void updateSelection()
    {
        auto selectedRows = audioTrack->getPlayListContainer()->playListItems.getSelectedRows();
        playListTableListBox->setSelectedRows(selectedRows, juce::dontSendNotification);
    }
    
    void updateUI(std::shared_ptr<AudioTrack> audioTrack_)
    {
        audioTrack = audioTrack_;
        playListTableListBoxModel->setAudioTrack(audioTrack_);
        
        updateSelection();
        playListTableListBox->updateContent();
        
        playListTableListBox->getHeader().setColumnName(1, audioTrack->getAudioTrackName());
        playListTableListBox->getHeader().setColour(juce::TableHeaderComponent::textColourId, audioTrack->getColour());
    }
    
    std::shared_ptr<AudioTrack> getAudioTrack() const { return audioTrack; }

private:
    
    std::shared_ptr<AudiumEngine> audiumEngine;
    std::shared_ptr<AudioTrack> audioTrack;
    std::shared_ptr<PlayListTableListBox> playListTableListBox;
    std::unique_ptr<PlayListTableListBoxModel> playListTableListBoxModel;
    
    bool itemDrag = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListComponent)
};
