/*
  ==============================================================================

    RegionTableListBox.h
    Created: 7 Jun 2023 2:03:54pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/*
*/
class RegionTableListBox  : public juce::TableListBox, juce::DragAndDropContainer
{
public:
    RegionTableListBox();
    ~RegionTableListBox() override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RegionTableListBox)
};
