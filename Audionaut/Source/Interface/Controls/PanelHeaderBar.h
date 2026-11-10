//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Interface/ColourIds.h"
#include "Interface/Controls/PanelToggleButton.h"

/**
 * The band across the top of a panel that opens downwards: the mixer under
 * the arrangement, the region list under the playlists. It is the seam to
 * the panel's neighbour above and the handle the panel is dragged by, and
 * it stays at the bottom while the panel is closed. It owns no layout: as
 * PanelResizerBar it reports the drag's distance from the mouse-down point,
 * and the owner turns that into the panel's state (CollapsiblePanel: pushed
 * well down, the panel collapses; pulled back up, it returns or opens; the
 * release decides). The cursor says so. At its right end, the panel's
 * toggle: it shows whether the panel is open (setOpen, by the owner) and
 * clicks go to onToggleClick.
 */
class PanelHeaderBar : public juce::Component
{
public:
    std::function<void()> onDragStart;
    std::function<void (int distance)> onDrag;   ///< pixels down from the mouse-down point
    std::function<void()> onDragEnd;             ///< the mouse-up, the moment to persist the state
    std::function<void()> onDoubleClick;
    std::function<void()> onToggleClick;         ///< the toggle at the right end: open or close the panel

    /// The name is the toggle button's, the tooltip what it says.
    PanelHeaderBar (const juce::String& panelName, const juce::String& toggleTooltip) :
        toggle (panelName, PanelToggleButton::Pane::bottom)
    {
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);

        toggle.setTooltip (toggleTooltip);
        toggle.setMouseCursor (juce::MouseCursor::NormalCursor);   // the band around it is the handle, the toggle is a button
        toggle.onClick = [this] { juce::NullCheckedInvocation::invoke (onToggleClick); };
        addAndMakeVisible (toggle);
    }

    /// What the toggle shows: whether the panel is open.
    void setOpen (bool open) { toggle.setToggleState (open, juce::dontSendNotification); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (findColour (audium::dividerColourId));

        // the seam to the neighbour above
        g.setColour (findColour (audium::backgroundColourId));
        g.fillRect (0, 0, getWidth(), seamThickness);
    }

    void resized() override
    {
        const auto band = getLocalBounds().withTrimmedTop (seamThickness);
        toggle.setBounds (juce::Rectangle<int> (toggleSize, toggleSize).withCentre ({ getWidth() - toggleMargin - toggleSize / 2, band.getCentreY() }));
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        juce::NullCheckedInvocation::invoke (onDragStart);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (onDrag != nullptr)
            onDrag (e.getDistanceFromDragStartY());
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
    static constexpr int seamThickness = 1;
    static constexpr int toggleSize = 20;
    static constexpr int toggleMargin = 12;

    PanelToggleButton toggle;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PanelHeaderBar)
};
