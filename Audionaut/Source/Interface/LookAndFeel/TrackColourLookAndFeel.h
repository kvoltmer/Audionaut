//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include "AudiumLookAndFeel.h"

class TrackColourLookAndFeel : public AudiumLookAndFeel
{
public:
    TrackColourLookAndFeel() = default;
    ~TrackColourLookAndFeel() override = default;
    
    // Table
    void drawTableHeaderColumn (juce::Graphics& g, juce::TableHeaderComponent& header,
                                                const juce::String& columnName, int /*columnId*/,
                                                int width, int height, bool isMouseOver, bool isMouseDown,
                                                int columnFlags) override;
    
};
