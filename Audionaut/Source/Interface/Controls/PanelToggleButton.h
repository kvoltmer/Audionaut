//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

/**
 * A header button that shows and hides one of the window's panels, drawn
 * like Xcode's inspector and debug-area toggles: the window as a rounded
 * rectangle, a divider where the panel sits, and the panel's part filled in
 * while the panel is shown (the toggle state, set by the owner).
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
        const auto active = shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown;
        const auto colour = juce::Colours::white.withAlpha (active ? 1.0f : 0.75f);
        const auto cornerSize = 2.5f;
        const auto lineThickness = 1.2f;

        auto window = getLocalBounds().toFloat().reduced (2.0f, 1.5f);
        auto panel = pane == Pane::right ? window.removeFromRight (window.getWidth() * 0.34f)
                                         : window.removeFromBottom (window.getHeight() * 0.4f);
        const auto outline = window.getUnion (panel);

        if (getToggleState()) {
            // rounded only at the window's own corners: the right pane has the two right ones, the bottom pane the two bottom ones
            juce::Path filled;
            const auto onTheRight = pane == Pane::right;
            filled.addRoundedRectangle (panel.getX(), panel.getY(), panel.getWidth(), panel.getHeight(),
                                        cornerSize, cornerSize,
                                        false, onTheRight,        // top-left, top-right
                                        ! onTheRight, true);      // bottom-left, bottom-right
            g.setColour (colour.withAlpha (colour.getFloatAlpha() * 0.4f));   // a tint, the outline stays the shape
            g.fillPath (filled);
        }

        g.setColour (colour);
        g.drawRoundedRectangle (outline, cornerSize, lineThickness);

        // the divider between the main area and the panel
        if (pane == Pane::right)
            g.drawLine (panel.getX(), outline.getY(), panel.getX(), outline.getBottom(), lineThickness);
        else
            g.drawLine (outline.getX(), panel.getY(), outline.getRight(), panel.getY(), lineThickness);
    }

private:
    const Pane pane;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PanelToggleButton)
};
