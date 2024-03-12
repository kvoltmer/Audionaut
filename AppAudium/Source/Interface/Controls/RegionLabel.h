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
#include "Engine/Group/AudioGroup.h"
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
                 std::shared_ptr<AudioRegionContainer> audioRegionContainer,
                 int columnId,
                 int rowNumber) :
        columnId(columnId),
        rowNumber(rowNumber),
        owner(owner),
        audioRegionContainer(audioRegionContainer)
    {
        setEditable (false, true, true);
        update (columnId, rowNumber, false);
        setFont(13.0f);
        addListener(this);
    }

    ~RegionLabel() override
    {
        removeListener(this);
    }
    
    void update(int columnId, int rowNumber, bool isSelected)
    {
        this->columnId = columnId;
        this->rowNumber = rowNumber;
        
        juce::String text = "n/a";
        if (AudioRegion* r = audioRegionContainer->getRegion(rowNumber).get())
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
            
            auto textColour = r->getAudioGroup()->getColour();
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
        auto audioRegion = audioRegionContainer->getRegion(rowNumber);
        auto action = std::make_unique<audium::UndoableContainerAction>(audioRegion);
        
        if (columnId == regionName)
        {
            audioRegionContainer->setRegionName(rowNumber, labelThatHasChanged->getText());
        }
        else if  (columnId == regionStart)
        {
            const auto seconds = labelThatHasChanged->getText().getDoubleValue();
            audioRegionContainer->setRegionStart(rowNumber, seconds);
        }
        else if  (columnId == regionEnd)
        {
            const auto seconds = labelThatHasChanged->getText().getDoubleValue();
            audioRegionContainer->setRegionEnd(rowNumber, seconds);
        }
        else if  (columnId == regionLength)
        {
            const auto seconds = labelThatHasChanged->getText().getDoubleValue();
            audioRegionContainer->setRegionLength(rowNumber, seconds);
        }
        
        // Undo: store new state
        action->storeNewState();
        audioRegionContainer->getUndoManager()->perform(action.release(), "Modify Region");
        audioRegionContainer->getUndoManager()->beginNewTransaction();
    }
    
    juce::String getRegionName() const
    {
        if (const AudioRegion* const r = audioRegionContainer->getRegion(rowNumber).get())
        {
            return r->getName();
        }
        return "n/a";
    }
    
    int getRowNumber() const
    {
        return rowNumber;
    }

private:
    int columnId;
    int rowNumber;
    std::shared_ptr<RegionTableListBox> owner;
    std::shared_ptr<AudioRegionContainer> audioRegionContainer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RegionLabel)
};
