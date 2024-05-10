/*
  ==============================================================================

    PlayListTableListBox.h
    Created: 28 Jun 2023 1:59:03pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class PlayListComponent;

class PlayListTableListBox : public juce::TableListBox {
    
    
public:
    PlayListTableListBox(PlayListComponent *owner_) :
        owner(owner_)
    {
    }
    
    ~PlayListTableListBox() override
    {
    }
    
    PlayListComponent *owner;
    
private:
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListTableListBox)
};
