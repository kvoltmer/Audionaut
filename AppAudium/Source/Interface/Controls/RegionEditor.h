/*
  ==============================================================================

    RegionNameTextEditor.h
    Created: 8 Jun 2023 5:45:45pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Engine/AudioRegion.h"
#include "Engine/AudioRegionContainer.h"
#include "Interface/Controls/RegionTableListBox.h"
#include "Interface/Models/RegionTableListBoxModel.h"

//==============================================================================
/*
*/
class RegionEditor  : public juce::Label, juce::Label::Listener
{
public:
    RegionEditor(std::shared_ptr<RegionTableListBox> owner,
                 std::shared_ptr<AudioRegionContainer> audioRegionContainer,
                 int columnId,
                 int rowNumber) :
        columnId(columnId),
        rowNumber(rowNumber),
        owner(owner),
        audioRegionContainer(audioRegionContainer)
    {
        setEditable (false, true, false);
        update (columnId, rowNumber, false);
        setFont(13.0f);
        addListener(this);
    }

    ~RegionEditor() override
    {
        removeListener(this);
    }
    
    void update(int columnId, int rowNumber, bool isSelected)
    {
        this->columnId = columnId;
        this->rowNumber = rowNumber;
        
        juce::String text = "n/a";
        if (const AudioRegion* const r = audioRegionContainer->getRegion(rowNumber).get())
        {
            if (columnId == regionName)
            {
                text = r->name;
            }
            else if (columnId == regionStart)
            {
                text = juce::String(r->position.getStart(), 4);
            }
            else if (columnId == regionEnd)
            {
                text = juce::String(r->position.getEnd(), 4);
            }
            else if (columnId == regionLength)
            {
                text = juce::String(r->position.getLength(), 4);
            }
        }
        setText (text, juce::dontSendNotification);
        
        if (isSelected)
            setColour (juce::Label::textColourId, findColour (audium::defaultHighlightedTextColourId));
        else
            setColour (juce::Label::textColourId, findColour (audium::defaultTextColourId));
    }
    
    /// this is odd but a click on the label should also select the row
    void mouseDown (const juce::MouseEvent& e) override
    {
        owner->selectRow(rowNumber);
        juce::Label::mouseDown(e);
    }
    
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if( juce::DragAndDropContainer* container = juce::DragAndDropContainer::findParentDragContainerFor(this))
        {
            container->startDragging("RegionEditor", this);
            //container->startDragging("PlayListTableListBoxItem", this);
        }
    }
    
    /// override juce::Label::Listener
    void labelTextChanged (juce::Label* labelThatHasChanged) override
    {
        if (columnId == regionName)
        {
            audioRegionContainer->setRegionName(rowNumber, labelThatHasChanged->getText());
        }
        else if  (columnId == regionStart)
        {
            audioRegionContainer->setRegionStart(rowNumber, labelThatHasChanged->getText().getDoubleValue());
        }
        else if  (columnId == regionEnd)
        {
            audioRegionContainer->setRegionEnd(rowNumber, labelThatHasChanged->getText().getDoubleValue());
        }
        else if  (columnId == regionLength)
        {
            audioRegionContainer->setRegionLength(rowNumber, labelThatHasChanged->getText().getDoubleValue());
        }
    }
    
    juce::String getRegionName() const
    {
        if (const AudioRegion* const r = audioRegionContainer->getRegion(rowNumber).get())
        {
            return r->name;
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RegionEditor)
};
