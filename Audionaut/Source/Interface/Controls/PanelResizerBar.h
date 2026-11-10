//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Interface/ColourIds.h"

/**
 * The seam between two panels that the user drags to resize them: a one
 * pixel line where the panels meet, a wider grab zone around it that the
 * cursor alone announces. A vertical bar sits between a left and a right
 * panel, a horizontal bar between a top and a bottom one.
 *
 * The bar owns no layout. It reports the drag distance from the mouse-down
 * point along its axis, and the owner turns that into a panel size, clamps
 * it and lays itself out again:
 *
 *     bar->onDragStart   = [this] { dragStartWidth = panelWidth; };
 *     bar->onDrag        = [this] (int distance) { setPanelWidth (dragStartWidth - distance); };
 *     bar->onDragEnd     = [this] { persist(); };
 *     bar->onDoubleClick = [this] { setPanelWidth (defaultWidth); persist(); };
 *
 * Lay the bar out with boundsForSeam(): centred on the seam, overlapping
 * both neighbours by a couple of pixels so it is easy to hit, and add it
 * after them so it stays on top.
 */
class PanelResizerBar : public juce::Component
{
public:
    static constexpr int seamThickness = 1;   ///< the gap between the two panels
    static constexpr int grabOverlap = 3;     ///< how far the bar reaches into each panel on either side of the seam

    explicit PanelResizerBar (bool isVertical) : vertical (isVertical)
    {
        setMouseCursor (vertical ? juce::MouseCursor::LeftRightResizeCursor
                                 : juce::MouseCursor::UpDownResizeCursor);
    }

    /// The bar's bounds for a seam between two panels (seamThickness wide or high).
    static juce::Rectangle<int> boundsForSeam (juce::Rectangle<int> seam, bool isVertical)
    {
        return isVertical ? seam.expanded (grabOverlap, 0) : seam.expanded (0, grabOverlap);
    }

    std::function<void()> onDragStart;
    std::function<void (int distance)> onDrag;   ///< pixels from the mouse-down point along the bar's axis
    std::function<void()> onDragEnd;             ///< the mouse-up, the moment to persist the size
    std::function<void()> onDoubleClick;         ///< typically: back to the default size

    void paint (juce::Graphics& g) override
    {
        auto line = getLocalBounds();
        line = vertical ? line.withSizeKeepingCentre (seamThickness, line.getHeight())
                        : line.withSizeKeepingCentre (line.getWidth(), seamThickness);

        g.setColour (findColour (audium::backgroundColourId));
        g.fillRect (line);
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        juce::NullCheckedInvocation::invoke (onDragStart);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (onDrag != nullptr)
            onDrag (vertical ? e.getDistanceFromDragStartX() : e.getDistanceFromDragStartY());
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        juce::NullCheckedInvocation::invoke (onDragEnd);   // a click included: the owner's drag state ends with the mouse-down
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        juce::NullCheckedInvocation::invoke (onDoubleClick);
    }

private:
    const bool vertical;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PanelResizerBar)
};
