//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Engine/Region/AudioRegion.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/Undo/UndoableContainerAction.h"
#include "Engine/Export/PlayListItemExport.h"

#include "Interface/Controls/RegionTableListBox.h"
#include "Interface/Models/RegionTableListBoxModel.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"

//==============================================================================
/*
*/
class [[deprecated]] TableRegionLabel  : public juce::Label, juce::Label::Listener
{
public:
    TableRegionLabel(std::shared_ptr<audium::AudiumEngine> audiumEngine_,
                     std::shared_ptr<audium::AudioTrackContainer> audioTrackContainer_,
                     int columnId_,
                     int rowNumber_) :
        audiumEngine(audiumEngine_),
        audioTrackContainer(audioTrackContainer_),
        columnId(columnId_),
        rowNumber(rowNumber_)
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
    
    void exportSelectedRegion();

private:
    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    std::shared_ptr<audium::AudioTrackContainer> audioTrackContainer;
    std::unique_ptr<audium::PlayListItemExport> exporter;
    
    int columnId = 1;
    int rowNumber = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TableRegionLabel)
};
