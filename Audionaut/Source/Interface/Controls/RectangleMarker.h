//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

/**
 * A transparent, mouse-transparent overlay that paints one rectangle in its
 * parent's coordinate space.
 *
 * Give it the full bounds of the area it marks (typically the parent's local
 * bounds) and move the rectangle with setRectangle(). Only the old and new
 * rectangle areas are invalidated, so a marker that moves every frame costs
 * a few pixels of repaint, not the whole parent.
 *
 * This replaces the pre-JUCE-9 pattern of adding a DrawableRectangle as a
 * child component: JUCE 9's DrawableComponent rounds its bounds to the
 * nearest pixel and paints unclipped, so the antialiased edges of a
 * sub-pixel line fall outside the component and are never cleared when it
 * moves, leaving a smear.
 */
class RectangleMarker : public juce::Component
{
public:
    RectangleMarker()
    {
        setInterceptsMouseClicks (false, false);
        setPaintingIsUnclipped (false);
    }

    void setFill (juce::Colour colour)
    {
        fillColour = colour;
        repaintRectangle (rectangle);
    }

    void setStroke (juce::Colour colour, float thickness)
    {
        strokeColour = colour;
        strokeThickness = thickness;
        repaintRectangle (rectangle);
    }

    /** Sets the rectangle to paint, in this component's local coordinates.
        An empty rectangle paints nothing. */
    void setRectangle (juce::Rectangle<float> newRectangle)
    {
        if (newRectangle == rectangle)
            return;

        auto old = rectangle;
        rectangle = newRectangle;
        repaintRectangle (old);
        repaintRectangle (rectangle);
    }

    juce::Rectangle<float> getRectangle() const { return rectangle; }

    void paint (juce::Graphics& g) override
    {
        if (rectangle.isEmpty())
            return;

        if (! fillColour.isTransparent())
        {
            g.setColour (fillColour);
            g.fillRect (rectangle);
        }

        if (strokeThickness > 0.0f && ! strokeColour.isTransparent())
        {
            g.setColour (strokeColour);
            g.drawRect (rectangle, strokeThickness);
        }
    }

private:
    void repaintRectangle (juce::Rectangle<float> r)
    {
        if (! r.isEmpty())
            repaint (r.getSmallestIntegerContainer().expanded (1));
    }

    juce::Rectangle<float> rectangle;
    juce::Colour fillColour = juce::Colours::transparentBlack;
    juce::Colour strokeColour = juce::Colours::transparentBlack;
    float strokeThickness = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RectangleMarker)
};
