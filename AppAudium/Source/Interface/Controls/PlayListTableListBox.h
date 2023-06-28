/*
  ==============================================================================

    PlayListTableListBox.h
    Created: 28 Jun 2023 1:59:03pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class PlayListTableListBox : public juce::TableListBox {
    
    
public:
    PlayListTableListBox()
    {
    }
    
    ~PlayListTableListBox() override
    {
    }
    
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListTableListBox)
};
