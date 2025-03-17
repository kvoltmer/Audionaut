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
                              std::shared_ptr<AudiumEngine> engine,
                              std::shared_ptr<AudioTrack> track) :
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
        audioTrack->getAudioTrackContainer().sendActionMessage(updateAll);
    }

    
    std::shared_ptr<PlayListTableListBox> listBox;
    
    std::shared_ptr<AudiumEngine> getAudiumEngine() const { return audiumEngine; }
    std::shared_ptr<PlayListContainer> getPlayListContainer() const { return audiumEngine->getPlayListContainer(audioTrack); }
    std::shared_ptr<PlayListScheduler> getPlayListScheduler() const { return audiumEngine->getPlayListScheduler(); }
    
    void setAudioTrack(std::shared_ptr<AudioTrack> audioTrack_) { audioTrack = audioTrack_; }
    std::shared_ptr<AudioTrack> getAudioTrack() const { return audioTrack; }
    
private:
    
    std::shared_ptr<AudiumEngine> audiumEngine;
    std::shared_ptr<AudioTrack> audioTrack;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListTableListBoxModel)
};
