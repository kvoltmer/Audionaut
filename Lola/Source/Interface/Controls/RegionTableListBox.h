//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

//==============================================================================
/*
*/
class RegionTableListBox  : public juce::TableListBox
{
public:
    RegionTableListBox();
    ~RegionTableListBox() override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RegionTableListBox)
};
