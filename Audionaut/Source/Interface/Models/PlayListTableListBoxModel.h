//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include "Engine/PlayList/PlayListItem.h"
#include "Engine/PlayList/PlayListContainer.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Region/AudioRegion.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Undo/UndoableContainerAction.h"

#include "Interface/Controls/PlayListTableListBox.h"
#include "Interface/ColourIds.h"
#include "Interface/Models/PlayListTableListBoxItem.h"

class PlayListTableListBoxModel : public juce::TableListBoxModel {
    
public:
    PlayListTableListBoxModel(std::shared_ptr<PlayListTableListBox> listBox,
                              std::shared_ptr<audium::AudiumEngine> engine,
                              std::shared_ptr<audium::AudioTrack> track) :
        listBox(listBox),
        audiumEngine(engine),
        audioTrack(track)
    {
    }
    
    int getNumRows() override
    {
        auto num = audioTrack->getPlayListContainer()->getNumItems();
        return num;
    }

    void paintRowBackground (juce::Graphics& g,
                                     int rowNumber,
                                     int width, int height,
                                     bool rowIsSelected) override
    {
        if (rowIsSelected)
        {
            g.fillAll (listBox->findColour(audium::listBoxBackgroundColourId));
        }
    }

    void paintCell (juce::Graphics& g,
                            int rowNumber,
                            int columnId,
                            int width, int height,
                            bool rowIsSelected) override
    {
    }

    juce::Component* refreshComponentForCell (int rowNumber, int columnId, bool isRowSelected,
                                                juce::Component* existingComponentToUpdate) override
    {
        if (existingComponentToUpdate == nullptr) {
            return new PlayListTableListBoxItem(this, columnId, rowNumber);
            
        }
        else {
            auto component = dynamic_cast<PlayListTableListBoxItem*>(existingComponentToUpdate);
            if (component != nullptr) {
                component->update(columnId, rowNumber, isRowSelected);
                return component;
            }
        }
        
        return nullptr;
    }

    void deleteKeyPressed (int lastRowSelected) override
    {
        audiumEngine->getAudioTrackContainer()->deleteSelectedObjects();
    }
    
    juce::var getDragSourceDescription (const juce::SparseSet< int > &rowsToDescribe) override
    {
        return "PlayListTableListBoxModel";
    }
    
    void selectedRowsChanged (int lastRowSelected) override
    {
        auto selectedRows = listBox->getSelectedRows();
        audioTrack->getPlayListContainer()->playListItems.setSelectedRows(selectedRows);
    }
    
    void backgroundClicked (const juce::MouseEvent&) override
    {
        listBox->deselectAllRows();
        audioTrack->getPlayListContainer()->playListItems.selectAllObjects(false);
        audioTrack->getAudioTrackContainer().sendActionMessage(audium::updateAll);
    }

    
    std::shared_ptr<PlayListTableListBox> listBox;
    
    std::shared_ptr<audium::AudiumEngine> getAudiumEngine() const { return audiumEngine; }
    std::shared_ptr<audium::PlayListContainer> getPlayListContainer() const { return audioTrack->getPlayListContainer(); }
    std::shared_ptr<audium::PlayListScheduler> getPlayListScheduler() const { return audiumEngine->getPlayListScheduler(); }
    
    void setAudioTrack(std::shared_ptr<audium::AudioTrack> audioTrack_) { audioTrack = audioTrack_; }
    std::shared_ptr<audium::AudioTrack> getAudioTrack() const { return audioTrack; }
    
private:
    
    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    std::shared_ptr<audium::AudioTrack> audioTrack;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListTableListBoxModel)
};
