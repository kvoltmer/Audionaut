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

#include "Engine/Region/AudioRegion.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/Undo/UndoableContainerAction.h"

#include "Interface/Controls/RegionTableListBox.h"
#include "Interface/Models/RegionTableListBoxModel.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"

//==============================================================================
/*
*/
class TableRegionLabel  : public juce::Label, juce::Label::Listener
{
public:
    TableRegionLabel(std::shared_ptr<audium::AudioTrackContainer> audioTrackContainer_,
                int columnId_,
                int rowNumber_) :
        columnId(columnId_),
        rowNumber(rowNumber_),
        audioTrackContainer(audioTrackContainer_)
    {
        setMinimumHorizontalScale(1.f);
        setEditable (false, true, false);
        update (columnId, rowNumber, false);
        setFont (juce::FontOptions (13.00f));
        addListener(this);
    }

    ~TableRegionLabel() override
    {
        removeListener(this);
    }
        
    const std::shared_ptr<audium::AudioRegion> getRegion(int columnId, int rowNumber) const
    {
        if (auto audioTrack = audioTrackContainer->getAudioTrack(columnId - 1)) {
            return audioTrack->getRegion(rowNumber);
        }
        return nullptr;
    }
    
    void update(int columnId, int rowNumber, bool isRowSelected);
    
    void mouseDown (const juce::MouseEvent& e) override;
    
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if( juce::DragAndDropContainer* container = juce::DragAndDropContainer::findParentDragContainerFor(this))
        {
            container->startDragging("TableRegionLabel", this);
        }
    }
    
    /// override juce::Label::Listener
    void labelTextChanged (juce::Label* labelThatHasChanged) override
    {
        // Undo: store old state
        
        if (auto audioRegion = getRegion(columnId, rowNumber)) {
            auto action = std::make_unique<audium::UndoableContainerAction>(*audioTrackContainer.get(), false);
            
            audioRegion->setName(labelThatHasChanged->getText());
            
            // Undo: store new state
            action->storeNewState();
            audioTrackContainer->getUndoManager()->perform(action.release(), "Rename Region");
            audioTrackContainer->getUndoManager()->beginNewTransaction();
        }
    }
    
    int getRowNumber() const
    {
        return rowNumber;
    }

private:
    int columnId = 1;
    int rowNumber = 0;
    
    std::shared_ptr<audium::AudioTrackContainer> audioTrackContainer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TableRegionLabel)
};
