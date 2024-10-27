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
        playListTableListBoxModel.reset(new PlayListTableListBoxModel(playListTableListBox, audiumEngine, track));

        playListTableListBox->setModel(playListTableListBoxModel.get());
        playListTableListBox->setMultipleSelectionEnabled(true);
        addAndMakeVisible(playListTableListBox.get());
        
        auto playListName = "Playlist - " + track->getName();
        playListTableListBox->getHeader().addColumn (playListName, 1, 250, 80, 800, juce::TableHeaderComponent::notSortable);
        playListTableListBox->getHeader().setStretchToFitActive (true);
        playListTableListBox->getHeader().setColour(juce::TableHeaderComponent::textColourId, track->getColour());
        playListTableListBox->setHeaderHeight(25);
        playListTableListBox->setOutlineThickness (0);
        playListTableListBox->updateContent();
    }

    ~PlayListComponent() override
    {
        playListTableListBox->setModel(nullptr);
        playListTableListBox = nullptr;
        playListTableListBoxModel = nullptr;
    }

    void paint (juce::Graphics& g) override
    {
    }

    void resized() override
    {
        playListTableListBox->setBounds(getLocalBounds());
    }
    
    void handleAsyncUpdate() override
    {
        playListTableListBox->updateContent();
    }
    
    bool isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails &dragSourceDetails) override;
    void itemDragEnter (const SourceDetails &dragSourceDetails) override
    {
        //updateInsertLines(dragSourceDetails);
    }
    
    void itemDragMove (const SourceDetails &dragSourceDetails) override
    {
        //updateInsertLines(dragSourceDetails);
    }
    
    void itemDragExit (const SourceDetails &dragSourceDetails) override
    {
        //hideInsertLines();
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
    
    void updateUI()
    {
        updateSelection();
        playListTableListBox->updateContent();
    }
    
    std::shared_ptr<AudioTrack> getAudioTrack() const { return audioTrack; }

private:
    
    std::shared_ptr<AudiumEngine> audiumEngine;
    std::shared_ptr<AudioTrack> audioTrack;
    std::shared_ptr<PlayListTableListBox> playListTableListBox;
    std::unique_ptr<PlayListTableListBoxModel> playListTableListBoxModel;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListComponent)
};
