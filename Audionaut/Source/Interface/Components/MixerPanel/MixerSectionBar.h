//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Interface/ColourIds.h"
#include "Interface/Components/MixerPanel/MixerStripComponent.h"

/**
 * The narrow bar at the mixer's left edge: one small icon per strip section
 * (I/O, inserts, sends, pan, the record / monitor / solo / mute buttons) that
 * shows or hides the section in every strip. The shown sections are a mask of
 * MixerLayout::Section; the owner keeps and persists it. The toggles stand
 * in a column spread over the strips' fader: the owner tells the bar where
 * the fader is (setFaderRange), and the first toggle's top and the last
 * one's bottom meet the fader's travel.
 */
class MixerSectionBar : public juce::Component
{
public:
    static constexpr int width = 30;

    std::function<void (int sections)> onChange;

    MixerSectionBar()
    {
        add (MixerLayout::Section::io, TRANS ("Input and output"));
        add (MixerLayout::Section::inserts, TRANS ("Inserts"));
        add (MixerLayout::Section::sends, TRANS ("Sends"));
        add (MixerLayout::Section::pan, TRANS ("Pan"));
        add (MixerLayout::Section::buttons, TRANS ("Record, monitor, solo and mute"));
    }

    int getSections() const noexcept { return sections; }

    void setSections (int newSections)
    {
        sections = newSections;
        for (auto& toggle : toggles)
            toggle->setToggleState ((sections & toggle->section) != 0, juce::dontSendNotification);
    }

    /// Where the strips' fader travels, in this bar's y; the toggles spread over it.
    void setFaderRange (juce::Range<int> range)
    {
        if (faderRange == range)
            return;
        faderRange = range;
        resized();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (findColour (audium::secondaryBackgroundColourId).darker (0.15f));
        g.setColour (findColour (audium::backgroundColourId));
        g.fillRect (getWidth() - 1, 0, 1, getHeight());
    }

    void resized() override
    {
        // a column over the fader's travel, in the order the sections appear in a strip from the top: the first
        // toggle's top on the fader's top, the last one's bottom on its bottom, the rest spread evenly between
        if (toggles.empty())
            return;
        const auto x = (getWidth() - 1 - toggleSize) / 2;
        const auto count = static_cast<int> (toggles.size());
        const auto span = juce::jmax (faderRange.getLength(), count * toggleSize);
        const auto step = count > 1 ? static_cast<float> (span - toggleSize) / static_cast<float> (count - 1) : 0.0f;
        for (int i = 0; i < count; ++i)
            toggles[static_cast<size_t> (i)]->setBounds (x, faderRange.getStart() + juce::roundToInt (i * step), toggleSize, toggleSize);
    }

private:
    static constexpr int toggleSize = 20;
    juce::Range<int> faderRange;

    /// One section's glyph, drawn like the buttons that show and hide the
    /// window's panels: fine rounded outlines with one solid part, in the
    /// accent colour while the section is shown and in the dimmed text
    /// colour while it is hidden.
    class Toggle : public juce::Button
    {
    public:
        Toggle (MixerLayout::Section section_, const juce::String& tooltip) :
            juce::Button (tooltip),
            section (section_)
        {
            setClickingTogglesState (false);   // the owner flips the mask and sets the state back
            setTooltip (tooltip);
        }

        void paintButton (juce::Graphics& g, bool highlighted, bool down) override
        {
            const auto active = highlighted || down;
            g.setColour (getToggleState() ? juce::Colour (audium::toggledOnColour).brighter (active ? 0.2f : 0.0f)
                                          : findColour (audium::defaultTextColourId).withAlpha (active ? 0.9f : 0.6f));
            drawGlyph (g, getLocalBounds().toFloat().withSizeKeepingCentre (glyphSize, glyphSize));
        }

        const MixerLayout::Section section;

    private:
        // the glyphs' measurements, to tune by eye
        static constexpr float glyphSize = 14.0f;
        static constexpr float lineThickness = 0.8f;   ///< the outlines' stroke
        static constexpr float strokeThickness = 1.0f; ///< the open lines' (arrows)
        static constexpr float cornerSize = 1.5f;
        static constexpr float barThickness = 2.0f;    ///< the solid bars
        static constexpr float arrowHead = 2.5f;

        void drawGlyph (juce::Graphics& g, juce::Rectangle<float> b) const
        {
            using namespace MixerLayout;

            switch (section) {
                case Section::io:
                {
                    // in at the top: an arrow into a bar; out at the bottom: an arrow away from one
                    const auto barHeight = 5.5f;
                    const auto y1 = b.getY() + barHeight / 2.0f + 0.25f;
                    const auto y2 = b.getBottom() - barHeight / 2.0f - 0.25f;
                    bar (g, { b.getRight() - barThickness, y1 - barHeight / 2.0f, barThickness, barHeight });
                    arrow (g, { b.getX() + 0.5f, y1 }, { b.getRight() - barThickness - 2.0f, y1 });
                    bar (g, { b.getX(), y2 - barHeight / 2.0f, barThickness, barHeight });
                    arrow (g, { b.getX() + barThickness + 2.0f, y2 }, { b.getRight() - 0.5f, y2 });
                    break;
                }

                case Section::inserts:
                {
                    // a column of slots, the first one taken
                    const auto gap = 1.75f;
                    const auto rowHeight = (b.getHeight() - 2.0f * gap) / 3.0f;
                    for (int i = 0; i < 3; ++i) {
                        const auto row = b.withHeight (rowHeight).translated (0.0f, static_cast<float> (i) * (rowHeight + gap));
                        if (i == 0)
                            g.fillRoundedRectangle (row, cornerSize);
                        else
                            g.drawRoundedRectangle (row.reduced (lineThickness / 2.0f), cornerSize, lineThickness);
                    }
                    break;
                }

                case Section::sends:
                {
                    // out of a bar, off to the right
                    bar (g, b.withWidth (barThickness).reduced (0.0f, 2.0f));
                    arrow (g, { b.getX() + barThickness + 2.5f, b.getCentreY() }, { b.getRight() - 0.5f, b.getCentreY() }, 3.0f);
                    break;
                }

                case Section::pan:
                {
                    // a knob: its rim, and a solid pointer at the centre position
                    const auto rim = b.reduced (lineThickness / 2.0f);
                    g.drawEllipse (rim, lineThickness);
                    const auto pointerInset = 2.75f;
                    bar (g, { b.getCentreX() - barThickness / 2.0f, b.getY() + pointerInset, barThickness, b.getHeight() / 2.0f - pointerInset + barThickness / 2.0f });
                    break;
                }

                case Section::buttons:
                {
                    // two rows of two, the first one lit
                    const auto gap = 2.0f;
                    const auto cellSize = (b.getWidth() - gap) / 2.0f;
                    for (int row = 0; row < 2; ++row)
                        for (int column = 0; column < 2; ++column) {
                            const auto cell = juce::Rectangle<float> (b.getX() + static_cast<float> (column) * (cellSize + gap),
                                                                      b.getY() + static_cast<float> (row) * (cellSize + gap), cellSize, cellSize);
                            if (row == 0 && column == 0)
                                g.fillRoundedRectangle (cell, cornerSize);
                            else
                                g.drawRoundedRectangle (cell.reduced (lineThickness / 2.0f), cornerSize, lineThickness);
                        }
                    break;
                }
            }
        }

        /// A solid bar with round ends.
        static void bar (juce::Graphics& g, juce::Rectangle<float> area)
        {
            g.fillRoundedRectangle (area, juce::jmin (area.getWidth(), area.getHeight()) / 2.0f);
        }

        /// A line to the right with an open head, its ends and corner rounded.
        static void arrow (juce::Graphics& g, juce::Point<float> from, juce::Point<float> to, float head = arrowHead)
        {
            juce::Path path;
            path.startNewSubPath (from);
            path.lineTo (to);
            path.startNewSubPath (to.x - head, to.y - head);
            path.lineTo (to);
            path.lineTo (to.x - head, to.y + head);
            g.strokePath (path, juce::PathStrokeType (strokeThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    };

    void add (MixerLayout::Section section, const juce::String& tooltip)
    {
        auto toggle = std::make_unique<Toggle> (section, tooltip);
        toggle->onClick = [this, section]
        {
            sections ^= section;
            setSections (sections);
            if (onChange)
                onChange (sections);
        };
        addAndMakeVisible (toggle.get());
        toggles.push_back (std::move (toggle));
    }

    int sections = MixerLayout::allSections;
    std::vector<std::unique_ptr<Toggle>> toggles;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerSectionBar)
};
