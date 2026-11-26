//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.


#pragma once

#include <JuceHeader.h>
#include "Engine/AudiumEngine.h"
#include "Engine/ActionMessages.h"
#include "Engine/Group/AudioTrack.h"

#include "Interface/Models/PlayListTableListBoxModel.h"
#include "Interface/Controls/PlayListTableListBox.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"

class PlayListComponent  : public juce::Component, public juce::DragAndDropTarget, public juce::AsyncUpdater
{
public:
    PlayListComponent(std::shared_ptr<audium::AudiumEngine> audiumEngine, std::shared_ptr<audium::AudioTrack> track) :
        audiumEngine(audiumEngine),
        audioTrack(track)
    {
        playListTableListBox.reset(new PlayListTableListBox(this));

        playListTableListBox->setMultipleSelectionEnabled(true);
        addAndMakeVisible(playListTableListBox.get());
        
        playListTableListBox->getHeader().addColumn ("", 1, 250, 0, 800,
                                                     juce::TableHeaderComponent::notResizableOrSortable);
        playListTableListBox->getHeader().setStretchToFitActive (true);
        
        playListTableListBox->setHeaderHeight(0);   // the table's header only lays out the column; the Header below shows the track
        playListTableListBox->setOutlineThickness (0);
        playListTableListBox->setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        
        playListTableListBoxModel = std::make_unique<PlayListTableListBoxModel> (playListTableListBox, audiumEngine, track);
        playListTableListBox->setModel(playListTableListBoxModel.get());

        header = std::make_unique<Header> (*this);
        addAndMakeVisible (header.get());

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
        auto bounds = getLocalBounds();
        header->setBounds (bounds.removeFromTop (AudiumLookAndFeel::tableHeaderHeight));
        playListTableListBox->setBounds (bounds);
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

        header->repaint();   // the selection tint
    }
    
    void updateUI(std::shared_ptr<audium::AudioTrack> audioTrack_)
    {
        audioTrack = audioTrack_;
        playListTableListBoxModel->setAudioTrack(audioTrack_);
        
        updateSelection();
        playListTableListBox->updateContent();
    }
    
    std::shared_ptr<audium::AudioTrack> getAudioTrack() const { return audioTrack; }

private:
    /**
     * The track's name over its playlist, drawn as the track's header in the
     * arrangement and the band over its mixer strips: the name in the
     * track's colour on the plain background, a tint of that colour while
     * the track is selected. A click selects the track as those do: plain
     * replaces the selection, Cmd adds or removes the track.
     */
    class Header : public juce::Component
    {
    public:
        explicit Header (PlayListComponent& owner_) : owner (owner_) {}

        void paint (juce::Graphics& g) override
        {
            auto& track = *owner.audioTrack;
            const auto colour = track.getViewState().getColour();
            if (track.isSelected())
                g.fillAll (colour.withAlpha (0.25f));
            g.setColour (juce::Colours::black);
            g.drawRect (getLocalBounds(), 1);
            g.setColour (colour);
            g.setFont (AudiumLookAndFeel::getTableHeaderFontOptions());   // as the region list's header under it
            g.drawText (track.getAudioTrackName() + " - Playlist", getLocalBounds().reduced (4, 0), juce::Justification::centredLeft, true);
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu())
                return;

            auto& track = *owner.audioTrack;
            if (! e.mods.isAnyModifierKeyDown() && ! track.isSelected())
                track.getSelectionManager()->deselectAll();
            track.setSelected (e.mods.isCommandDown() ? ! track.isSelected() : true);
            track.getAudioTrackContainer().sendActionMessage (audium::updateAll);
        }

    private:
        PlayListComponent& owner;
    };

    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    std::shared_ptr<audium::AudioTrack> audioTrack;
    std::unique_ptr<Header> header;
    std::shared_ptr<PlayListTableListBox> playListTableListBox;
    std::unique_ptr<PlayListTableListBoxModel> playListTableListBoxModel;
    
    bool itemDrag = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListComponent)
};
