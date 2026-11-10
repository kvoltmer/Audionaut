//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Interface/ColourIds.h"

/**
 * A button that shows and hides one of the window's panels: a glyph of the
 * window's rounded outline with a solid bar inset at the panel's side, in
 * the accent colour while the panel is shown (the toggle state, set by the
 * owner) and in the dimmed text colour while it is hidden.
 */
class PanelToggleButton : public juce::Button
{
public:
    enum class Pane { right, bottom };

    PanelToggleButton (const juce::String& name, Pane pane_) :
        juce::Button (name),
        pane (pane_)
    {
        setClickingTogglesState (false);   // the owner knows whether the panel is shown
    }

    void paintButton (juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        paintGlyph (g, shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown);
    }

private:
    /// The window's rounded outline and a solid bar inset at the panel's side.
    void paintGlyph (juce::Graphics& g, bool active) const
    {
        // the glyph's measurements, to tune by eye
        const auto glyphWidth = 15.0f;
        const auto glyphHeight = 14.0f;
        const auto cornerSize = 1.5f;        ///< the window outline's corners
        const auto lineThickness = 0.8f;     ///< the outline's stroke
        const auto barInset = 2.75f;         ///< the gap between the outline and the bar
        const auto barShare = 0.32f;         ///< the bar's share of the window's inside
        const auto barCornerSize = 1.5f;

        const auto on = getToggleState();
        const auto colour = on ? juce::Colour (audium::toggledOnColour).brighter (active ? 0.2f : 0.0f)
                               : findColour (audium::defaultTextColourId).withAlpha (active ? 0.9f : 0.6f);
        const auto window = getLocalBounds().toFloat().withSizeKeepingCentre (glyphWidth, glyphHeight).reduced (lineThickness / 2.0f);

        g.setColour (colour);
        g.drawRoundedRectangle (window, cornerSize, lineThickness);

        auto bar = window.reduced (barInset);
        bar = pane == Pane::right ? bar.removeFromRight (bar.getWidth() * barShare)
                                  : bar.removeFromBottom (bar.getHeight() * barShare);
        g.fillRoundedRectangle (bar, barCornerSize);
    }

    const Pane pane;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PanelToggleButton)
};
