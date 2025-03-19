//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Lola uses a GPL/commercial licence - see LICENCE.md for details.

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
