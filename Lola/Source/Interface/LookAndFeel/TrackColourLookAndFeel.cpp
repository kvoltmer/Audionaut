/*
  ==============================================================================

    TrackColourLookAndFeel.cpp
    Created: 13 Feb 2025 4:00:24pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "TrackColourLookAndFeel.h"

using namespace juce;

void TrackColourLookAndFeel::drawTableHeaderColumn (Graphics& g, TableHeaderComponent& header,
                                                    const String& columnName, int columnId,
                                                    int width, int height, bool isMouseOver, bool isMouseDown,
                                                    int columnFlags)
{
    auto highlightColour = header.findColour (TableHeaderComponent::highlightColourId);

    if (isMouseDown)
        g.fillAll (highlightColour);
    else if (isMouseOver)
        g.fillAll (highlightColour.withMultipliedAlpha (0.625f));

    Rectangle<int> area (width, height);
    area.reduce (4, 0);

    if ((columnFlags & (TableHeaderComponent::sortedForwards | TableHeaderComponent::sortedBackwards)) != 0)
    {
        Path sortArrow;
        sortArrow.addTriangle (0.0f, 0.0f,
                               0.5f, (columnFlags & TableHeaderComponent::sortedForwards) != 0 ? -0.8f : 0.8f,
                               1.0f, 0.0f);

        g.setColour (Colour (0x99000000));
        g.fillPath (sortArrow, sortArrow.getTransformToScaleToFit (area.removeFromRight (height / 2).reduced (2).toFloat(), true));
    }

    // !!! set the text colour of the column
    jassert(columnId > 0);
    g.setColour (trackColours[columnId - 1]);
    
    g.setFont (withDefaultMetrics (FontOptions ((float) height * 0.5f, Font::bold)));
    g.drawFittedText (columnName, area, Justification::centredLeft, 1, 1.f);
}
