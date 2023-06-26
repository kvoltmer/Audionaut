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
#include "Interface/Controls/RegionTableListBox.h"

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
        juce::String text = "n/a";
        if (const AudioRegion* const r = audioRegionContainer->getRegion(rowNumber).get())
        {
            if (columnId == 1)
            {
                text = r->name;
            }
            else if (columnId == 2)
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
    
    /// override juce::Label::Listener
    void labelTextChanged (juce::Label* labelThatHasChanged) override
    {
        if (columnId == 1)
        {
            audioRegionContainer->setRegionName(rowNumber, labelThatHasChanged->getText());
        }
        else if  (columnId == 2)
        {
            audioRegionContainer->setRegionLength(rowNumber, labelThatHasChanged->getText().getDoubleValue());
        }
    }

private:
    int columnId;
    int rowNumber;
    std::shared_ptr<RegionTableListBox> owner;
    std::shared_ptr<AudioRegionContainer> audioRegionContainer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RegionEditor)
};
