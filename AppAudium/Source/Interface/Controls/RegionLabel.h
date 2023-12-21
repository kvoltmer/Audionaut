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
#include "Engine/Group/AudioGroup.h"
#include "Engine/AudioRegionContainer.h"
#include "Interface/Controls/RegionTableListBox.h"
#include "Interface/Models/RegionTableListBoxModel.h"
#include "Engine/PlayList/PlayListScheduler.h"


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
        setEditable (false, true, false);
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
                text = r->name;
            }
            else if (columnId == regionStart)
            {
                const auto seconds = r->getRegionDataInSeconds().getStart();
                text = juce::String(seconds, 4);
            }
            else if (columnId == regionEnd)
            {
                const auto seconds = r->getRegionDataInSeconds().getEnd();
                text = juce::String(seconds, 4);
            }
            else if (columnId == regionLength)
            {
                const auto seconds = r->getRegionDataInSeconds().getLength();
                text = juce::String(seconds, 4);
            }
            
            auto textColour = r->getAudioGroup()->getColour();
            setColour (juce::Label::textColourId, isSelected ? textColour.brighter() : textColour);
            
        }
        setText (text, juce::dontSendNotification);
        

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
            container->startDragging("RegionLabel", this);
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
            const auto seconds = labelThatHasChanged->getText().getDoubleValue();
            const auto clocks = audioRegionContainer->getPlayListScheduler()->getTempoProvider()->secondsToClocks(seconds);
            audioRegionContainer->setRegionStart(rowNumber, clocks);
        }
        else if  (columnId == regionEnd)
        {
            const auto seconds = labelThatHasChanged->getText().getDoubleValue();
            const auto clocks = audioRegionContainer->getPlayListScheduler()->getTempoProvider()->secondsToClocks(seconds);
            audioRegionContainer->setRegionEnd(rowNumber, clocks);
        }
        else if  (columnId == regionLength)
        {
            const auto seconds = labelThatHasChanged->getText().getDoubleValue();
            const auto clocks = audioRegionContainer->getPlayListScheduler()->getTempoProvider()->secondsToClocks(seconds);
            audioRegionContainer->setRegionLength(rowNumber, clocks);
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RegionLabel)
};
