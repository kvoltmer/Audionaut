//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Interface/Components/MixerPanel/MixerContext.h"

/**
 * The editor of one insert slot, generated from the effect's parameter
 * list: a row per parameter with a label and a slider (a toggle for a
 * boolean), and a bypass toggle on top. Lives in a non-modal window the
 * mixer owns, one per slot.
 *
 * A slider drag writes the live value into the effect and records one undo
 * step from the value before the drag to the value after; a typed or
 * double-clicked value is one step by itself. The rows follow the effect
 * on a timer, so a change from elsewhere (an agent, undo) shows up.
 *
 * The slot is held weakly: once it left its chain or the project, the
 * editor asks its owner to close the window.
 */
class EffectEditorComponent : public juce::Component,
                              private juce::Timer
{
public:
    static constexpr int width = 360;
    static constexpr int rowHeight = 24;
    static constexpr int headerHeight = 28;
    static constexpr int labelWidth = 100;
    static constexpr int margin = 8;

    EffectEditorComponent (MixerContext context, audium::InsertChainAddress address, std::shared_ptr<audium::EffectSlot> slot);
    ~EffectEditorComponent() override;

    /// The window's content height for this effect.
    int getPreferredHeight() const;

    /// The slot is still in its chain and the chain still in the project.
    bool isStillValid() const;

    /// Asked when the slot went away; the owner closes the window (asynchronously).
    std::function<void()> onSlotGone;

    void resized() override;
    void paint (juce::Graphics& g) override;

private:
    struct Row
    {
        int parameter = 0;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::ToggleButton> toggle;
        bool dragging = false;
        float valueAtDragStart = 0.0f;
    };

    void timerCallback() override;
    void refresh();
    int slotIndex() const;
    void commit (int parameter, float oldValue, float newValue);

    MixerContext context;
    audium::InsertChainAddress address;
    std::weak_ptr<audium::EffectSlot> slotRef;
    juce::String effectName;

    std::unique_ptr<juce::ToggleButton> bypassToggle;
    std::vector<std::unique_ptr<Row>> rows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectEditorComponent)
};
