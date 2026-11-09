//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Interface/ColourIds.h"
#include "Interface/Controls/PanelToggleButton.h"

/**
 * The band across the mixer's top: the seam to the arrangement above it,
 * and the handle the mixer is dragged by. It stays at
 * the bottom while the mixer is closed. It owns no layout: as
 * PanelResizerBar it reports the drag's distance from the mouse-down point,
 * and the owner turns that into the mixer's state (CollapsiblePanel: pushed
 * well down, the mixer collapses; pulled back up, it returns or opens; the
 * release decides). The cursor says so. At its right end, the mixer's
 * toggle: it shows whether the mixer is open (setOpen, by the owner) and
 * clicks go to onToggleClick.
 */
class MixerHeaderBar : public juce::Component
{
public:
    std::function<void()> onDragStart;
    std::function<void (int distance)> onDrag;   ///< pixels down from the mouse-down point
    std::function<void()> onDragEnd;             ///< the mouse-up, the moment to persist the state
    std::function<void()> onDoubleClick;
    std::function<void()> onToggleClick;         ///< the toggle at the right end: open or close the mixer

    MixerHeaderBar()
    {
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);

        toggle.setTooltip (TRANS ("Show or hide the mixer"));
        toggle.setMouseCursor (juce::MouseCursor::NormalCursor);   // the band around it is the handle, the toggle is a button
        toggle.onClick = [this] { juce::NullCheckedInvocation::invoke (onToggleClick); };
        addAndMakeVisible (toggle);
    }

    /// What the toggle shows: whether the mixer is open.
    void setOpen (bool open) { toggle.setToggleState (open, juce::dontSendNotification); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (findColour (audium::dividerColourId));

        // the seam to the arrangement
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

    PanelToggleButton toggle { "Mixer", PanelToggleButton::Pane::bottom };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerHeaderBar)
};
