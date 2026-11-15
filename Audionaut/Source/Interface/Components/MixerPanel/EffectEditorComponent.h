//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Interface/Components/MixerPanel/MixerContext.h"
#include "Interface/Components/MixerPanel/SlotEditor.h"

/**
 * The editor of one insert slot, generated from the effect's parameter
 * list: a grid of console dials, each with its name above and its value
 * below (a key for a boolean), nothing else - the window is the effect.
 * Bypass is the slot's (its menu in the mixer); while the slot is bypassed
 * the dials are dimmed, still live. Lives in a non-modal window the mixer
 * owns, one per slot. Parameters with a group
 * (the EQ's bands) stand in a column each under the group's name, labelled
 * within it ("Gain", "Freq"); without groups the dials fill rows.
 *
 * A dial's value arcs from zero where the range crosses it (a gain in dB),
 * from the bottom otherwise; the mark sits at the default, where a
 * double-click returns. A double-click on the value types one in. A
 * frequency dial turns on a logarithmic taper.
 *
 * An effect that reports its gain reduction (the compressor) gets a meter
 * in a strip under the dials: a bar growing from the left on a linear dB
 * scale, holding the peak of each timer interval and falling at a steady
 * rate between readings.
 *
 * A dial drag writes the live value into the effect and records one undo
 * step from the value before the drag to the value after; a typed or
 * double-clicked value is one step by itself. A parameter that moves the
 * effect's latency (the compressor's look-ahead) is written at the drag's
 * end only, as every value would rebuild the plan around the slot. The
 * dials follow the effect on a timer, so a change from elsewhere (an
 * agent, undo) shows up.
 *
 * The slot is held weakly: once it left its chain or the project, the
 * editor asks its owner to close the window.
 */
class EffectEditorComponent : public juce::Component,
                              public SlotEditor,
                              private juce::Timer
{
public:
    static constexpr int meterHeight = 30;   ///< the strip under the dials, where the effect has a meter
    static constexpr int groupHeight = 20;
    static constexpr int cellWidth = 64;
    static constexpr int nameHeight = 15;
    static constexpr int dialHeight = 38;
    static constexpr int valueHeight = 16;
    static constexpr int cellHeight = nameHeight + dialHeight + valueHeight;
    static constexpr int margin = 10;
    static constexpr int minimumWidth = 200;
    static constexpr int maximumColumns = 6;   ///< more parameters than that go on two rows
    static constexpr int minimumWidthWithMeter = 300;   ///< room in the strip for the meter's name and a readable scale
    static constexpr float meterRangeDb = 24.0f;   ///< the scale's full extent
    static constexpr float meterFallDbPerTick = 1.0f;   ///< the bar's fall between readings, 25 ticks a second

    EffectEditorComponent (MixerContext context, audium::InsertChainAddress address, std::shared_ptr<audium::EffectSlot> slot);
    ~EffectEditorComponent() override;

    /// The window's content size for this effect.
    int getPreferredWidth() const;
    int getPreferredHeight() const;

    bool isStillValid() const override;

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
        bool heldWhileDragging = false;   ///< written at the drag's end only: the value moves the latency
        float valueAtDragStart = 0.0f;
    };

    void timerCallback() override;
    void refresh();
    void refreshMeter();
    void paintMeter (juce::Graphics& g) const;
    int slotIndex() const;
    void commit (int parameter, float oldValue, float newValue);
    void showValue (Cell& cell);
    void arrange();
    juce::Rectangle<int> gridArea() const;

    MixerContext context;
    audium::InsertChainAddress address;
    std::weak_ptr<audium::EffectSlot> slotRef;
    juce::String effectName;

    std::vector<std::unique_ptr<Cell>> cells;
    std::vector<juce::String> groups;   ///< each column's heading, empty when the effect has no groups
    int columns = 1, rows = 1;

    bool meterShown = false;
    float meterDb = 0.0f;   ///< the bar's current height, after hold and fall
    juce::Rectangle<int> meterArea;   ///< the strip under the dials; empty without a meter

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectEditorComponent)
};
