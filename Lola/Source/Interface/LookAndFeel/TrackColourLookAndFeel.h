/*
  ==============================================================================

    TrackColourLookAndFeel.h
    Created: 13 Feb 2025 4:00:24pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

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
