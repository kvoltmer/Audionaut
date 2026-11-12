//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Interface/Components/MixerPanel/MixerContext.h"

/**
 * The editor of one insert slot, generated from the effect's parameter
 * list: a grid of console dials, each with its name above and its value
 * below (a key for a boolean), and a bypass key in a band on top. Lives in
 * a non-modal window the mixer owns, one per slot. Parameters with a group
 * (the EQ's bands) stand in a column each under the group's name, labelled
 * within it ("Gain", "Freq"); without groups the dials fill rows.
 *
 * A dial's value arcs from zero where the range crosses it (a gain in dB),
 * from the bottom otherwise; the mark sits at the default, where a
 * double-click returns. A double-click on the value types one in. A
 * frequency dial turns on a logarithmic taper. While the slot is
 * bypassed the dials are dimmed, still live.
 *
 * A dial drag writes the live value into the effect and records one undo
 * step from the value before the drag to the value after; a typed or
 * double-clicked value is one step by itself. The dials follow the effect
 * on a timer, so a change from elsewhere (an agent, undo) shows up.
 *
 * The slot is held weakly: once it left its chain or the project, the
 * editor asks its owner to close the window.
 */
class EffectEditorComponent : public juce::Component,
                              private juce::Timer
{
public:
    static constexpr int headerHeight = 30;
    static constexpr int groupHeight = 20;
    static constexpr int cellWidth = 64;
    static constexpr int nameHeight = 15;
    static constexpr int dialHeight = 38;
    static constexpr int valueHeight = 16;
    static constexpr int cellHeight = nameHeight + dialHeight + valueHeight;
    static constexpr int margin = 10;
    static constexpr int minimumWidth = 200;
    static constexpr int maximumColumns = 6;   ///< more parameters than that go on two rows

    EffectEditorComponent (MixerContext context, audium::InsertChainAddress address, std::shared_ptr<audium::EffectSlot> slot);
    ~EffectEditorComponent() override;

    /// The window's content size for this effect.
    int getPreferredWidth() const;
    int getPreferredHeight() const;

    /// The slot is still in its chain and the chain still in the project.
    bool isStillValid() const;

    /// Asked when the slot went away; the owner closes the window (asynchronously).
    std::function<void()> onSlotGone;

    void resized() override;
    void paint (juce::Graphics& g) override;

private:
    struct Cell
    {
        int parameter = 0;
        int column = 0, row = 0;
        juce::String name;
        juce::Rectangle<int> nameArea;
        std::unique_ptr<juce::Slider> dial;
        std::unique_ptr<juce::Label> value;
        std::unique_ptr<juce::TextButton> key;
        bool dragging = false;
        float valueAtDragStart = 0.0f;
    };

    void timerCallback() override;
    void refresh();
    int slotIndex() const;
    void commit (int parameter, float oldValue, float newValue);
    void showValue (Cell& cell);
    void arrange();
    juce::Rectangle<int> gridArea() const;

    MixerContext context;
    audium::InsertChainAddress address;
    std::weak_ptr<audium::EffectSlot> slotRef;
    juce::String effectName;

    juce::TextButton bypassKey;
    std::vector<std::unique_ptr<Cell>> cells;
    std::vector<juce::String> groups;   ///< each column's heading, empty when the effect has no groups
    int columns = 1, rows = 1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectEditorComponent)
};
