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
#include "Interface/Controls/RegionTableListBox.h"
#include "Interface/Models/RegionTableListBoxModel.h"
#include "Engine/PlayList/PlayListScheduler.h"
#include "Engine/Undo/UndoableContainerAction.h"

//==============================================================================
/*
*/
class RegionLabel  : public juce::Label, juce::Label::Listener
{
public:
    RegionLabel(std::shared_ptr<RegionTableListBox> owner,
                 std::shared_ptr<AudioTrackContainer> audioTrackContainer,
                 int columnId,
                 int rowNumber) :
        columnId(columnId),
        rowNumber(rowNumber),
        owner(owner),
        audioTrackContainer(audioTrackContainer)
    {
        setEditable (false, true, false);
        update (columnId, rowNumber, false);
        setFont (juce::FontOptions (13.00f));
        addListener(this);
    }

    ~RegionLabel() override
    {
        removeListener(this);
    }
    
    const std::shared_ptr<AudioRegion> getRegion() const
    {
        return audioTrackContainer->getAudioRegionAdapter().getRegion(getRowNumber());
    }
    
    void update(int columnId, int rowNumber, bool isSelected)
    {
        this->columnId = columnId;
        this->rowNumber = rowNumber;
        
        juce::String text = "n/a";
        auto audioRegions = audioTrackContainer->getAudioRegionAdapter().getAudioRegions();
        jassert(rowNumber < audioRegions.size());
        if (AudioRegion* r = audioRegions[rowNumber].get())
        {
            if (columnId == regionName)
            {
                text = r->getName();
            }
            else if (columnId == regionStart)
            {
                const auto seconds = r->getRegionData(audium::seconds).getStart();
                text = juce::String(seconds, 4);
            }
            else if (columnId == regionEnd)
            {
                const auto seconds = r->getRegionData(audium::seconds).getEnd();
                text = juce::String(seconds, 4);
            }
            else if (columnId == regionLength)
            {
                const auto seconds = r->getRegionData(audium::seconds).getLength();
                text = juce::String(seconds, 4);
            }
            
            auto textColour = r->getAudioTrack()->getColour();
            setColour (juce::Label::textColourId, isSelected ? textColour.brighter() : textColour);
            
        }
        setText (text, juce::dontSendNotification);
        

    }
    
    /// pass on mouse events. unless row is not selected
    void mouseDown (const juce::MouseEvent& e) override
    {
        getParentComponent()->mouseDown(e);
    }
    
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if( juce::DragAndDropContainer* container = juce::DragAndDropContainer::findParentDragContainerFor(this))
        {
            container->startDragging("RegionLabel", this);
            //container->startDragging("PlayListTableListBoxItem", this);
        }
    }
    
    /// override juce::Label::Listener
    void labelTextChanged (juce::Label* labelThatHasChanged) override
    {
        // Undo: store old state
        auto audioRegions = audioTrackContainer->getAudioRegionAdapter().getAudioRegions();
        jassert(rowNumber < audioRegions.size());
        auto audioRegion = audioRegions[rowNumber];
        auto action = std::make_unique<audium::UndoableContainerAction>(*audioTrackContainer.get(), false);
        
        if (columnId == regionName)
        {
            audioRegion->setName(labelThatHasChanged->getText());
        }
        else if  (columnId == regionStart)
        {
            const auto seconds = labelThatHasChanged->getText().getDoubleValue();
            audioRegion->setRegionStart(seconds, audium::seconds);
        }
        else if  (columnId == regionEnd)
        {
            const auto seconds = labelThatHasChanged->getText().getDoubleValue();
            audioRegion->setRegionEnd(seconds, audium::seconds);
        }
        else if  (columnId == regionLength)
        {
            const auto seconds = labelThatHasChanged->getText().getDoubleValue();
            audioRegion->setRegionLength(seconds, audium::seconds);
        }
        
        // Undo: store new state
        action->storeNewState();
        audioTrackContainer->getUndoManager()->perform(action.release(), "Modify Region");
        audioTrackContainer->getUndoManager()->beginNewTransaction();
    }
    
    int getRowNumber() const
    {
        return rowNumber;
    }

private:
    int columnId;
    int rowNumber;
    std::shared_ptr<RegionTableListBox> owner;
    
    std::shared_ptr<AudioTrackContainer> audioTrackContainer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RegionLabel)
};
