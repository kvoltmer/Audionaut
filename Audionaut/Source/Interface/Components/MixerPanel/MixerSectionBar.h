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

    /// One section's icon on a small plate: the plate lit grey while the
    /// section is shown, dark while it is hidden.
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
            const auto on = getToggleState();
            const auto active = highlighted || down;
            const auto text = findColour (audium::defaultTextColourId);
            const auto plate = getLocalBounds().toFloat().reduced (0.5f);

            // the plate: a lit grey key while on (not the accent blue of the transport toggles), a dark one while off
            if (on)
                g.setColour (text.withAlpha (active ? 0.45f : 0.32f));
            else
                g.setColour (findColour (audium::backgroundColourId).darker (0.3f).withAlpha (active ? 0.7f : 1.0f));
            g.fillRoundedRectangle (plate, 3.0f);
            g.setColour (juce::Colours::black.withAlpha (on ? 0.45f : 0.3f));
            g.drawRoundedRectangle (plate, 3.0f, 1.0f);

            const auto colour = text.withAlpha (on ? 0.95f : (active ? 0.6f : 0.4f));
            g.setColour (colour);
            drawIcon (g, colour, getLocalBounds().toFloat().withSizeKeepingCentre (13.0f, 13.0f), on);
        }

        const MixerLayout::Section section;

    private:
        void drawIcon (juce::Graphics& g, juce::Colour colour, juce::Rectangle<float> b, bool on) const
        {
            using namespace MixerLayout;
            const auto stroke = 1.1f;
            const auto fillAlpha = on ? 0.3f : 0.0f;

            switch (section) {
                case Section::inserts:
                {
                    // a column of slots
                    auto slots = b.reduced (1.5f, 1.0f);
                    const auto rowHeight = slots.getHeight() / 3.0f;
                    for (int i = 0; i < 3; ++i) {
                        const auto row = slots.removeFromTop (rowHeight).reduced (0.0f, 0.9f);
                        g.setColour (colour.withMultipliedAlpha (fillAlpha));
                        g.fillRoundedRectangle (row, 1.0f);
                        g.setColour (colour);
                        g.drawRoundedRectangle (row, 1.0f, stroke);
                    }
                    break;
                }

                case Section::sends:
                {
                    // out of a bar, off to the right
                    const auto bar = juce::Rectangle<float> (b.getX() + 1.0f, b.getY() + 2.5f, 1.8f, b.getHeight() - 5.0f);
                    g.setColour (colour.withMultipliedAlpha (on ? 1.0f : 0.8f));
                    g.fillRoundedRectangle (bar, 1.0f);
                    arrow (g, { b.getX() + 5.0f, b.getCentreY() }, { b.getRight() - 1.0f, b.getCentreY() }, stroke);
                    break;
                }

                case Section::io:
                {
                    // in at the top: an arrow into a bar; out at the bottom: an arrow away from one
                    const auto y1 = b.getY() + 3.5f;
                    const auto y2 = b.getBottom() - 3.5f;
                    g.fillRoundedRectangle (b.getRight() - 3.0f, y1 - 2.5f, 1.8f, 5.0f, 1.0f);
                    arrow (g, { b.getX() + 0.5f, y1 }, { b.getRight() - 4.5f, y1 }, stroke);
                    g.fillRoundedRectangle (b.getX() + 1.0f, y2 - 2.5f, 1.8f, 5.0f, 1.0f);
                    arrow (g, { b.getX() + 4.5f, y2 }, { b.getRight() - 0.5f, y2 }, stroke);
                    break;
                }

                case Section::pan:
                {
                    // a knob: an open arc with its pointer at the top
                    const auto centre = juce::Point<float> (b.getCentreX(), b.getCentreY() + 1.0f);
                    const auto radius = b.getWidth() * 0.4f;
                    juce::Path arc;   // angles run clockwise from the top: from 7 o'clock over the top to 5 o'clock
                    arc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                                       -juce::MathConstants<float>::pi * 0.75f, juce::MathConstants<float>::pi * 0.75f, true);
                    if (on) {
                        juce::Path disc;
                        disc.addEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
                        g.setColour (colour.withMultipliedAlpha (fillAlpha));
                        g.fillPath (disc);
                        g.setColour (colour);
                    }
                    g.strokePath (arc, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                    g.drawLine (centre.x, centre.y, centre.x, centre.y - radius + 0.5f, stroke + 0.3f);
                    break;
                }

                case Section::buttons:
                {
                    // two rows of two
                    auto grid = b.reduced (1.0f, 2.0f);
                    const auto cellWidth = grid.getWidth() / 2.0f;
                    const auto cellHeight = grid.getHeight() / 2.0f;
                    for (int row = 0; row < 2; ++row)
                        for (int column = 0; column < 2; ++column) {
                            const auto cell = juce::Rectangle<float> (grid.getX() + column * cellWidth, grid.getY() + row * cellHeight, cellWidth, cellHeight).reduced (1.0f);
                            g.setColour (colour.withMultipliedAlpha (fillAlpha));
                            g.fillRoundedRectangle (cell, 1.0f);
                            g.setColour (colour);
                            g.drawRoundedRectangle (cell, 1.0f, stroke);
                        }
                    break;
                }
            }
        }

        static void arrow (juce::Graphics& g, juce::Point<float> from, juce::Point<float> to, float stroke)
        {
            g.drawLine (from.x, from.y, to.x - 1.0f, to.y, stroke);
            juce::Path head;
            head.addTriangle (to.x, to.y, to.x - 3.5f, to.y - 2.5f, to.x - 3.5f, to.y + 2.5f);
            g.fillPath (head);
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
