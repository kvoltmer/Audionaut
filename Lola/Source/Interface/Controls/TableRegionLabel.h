/*
  ==============================================================================

    RegionNameTextEditor.h
    Created: 8 Jun 2023 5:45:45pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

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
    TableRegionLabel(std::shared_ptr<AudioTrackContainer> audioTrackContainer_,
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
        
    const std::shared_ptr<AudioRegion> getRegion(int columnId, int rowNumber) const
    {
        if (auto audioTrack = audioTrackContainer->getAudioTrack(columnId - 1)) {
            return audioTrack->getAudioRegionContainer()->getRegion(rowNumber);
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
    
    std::shared_ptr<AudioTrackContainer> audioTrackContainer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TableRegionLabel)
};
