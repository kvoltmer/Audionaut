//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Interface/Components/MixerPanel/MixerContext.h"
#include "Interface/Components/MixerPanel/MixerLayout.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"
#include "Engine/Undo/UndoableSendAction.h"
#include "Interface/Controls/LevelMeter.h"

/**
 * One bus channel's send into one aux bus, as a knob like the pan knob: the
 * aux's name over a dial on the fader's taper up to +6 dB (the arc from silence
 * up to the level, the mark at silence, the default). Every channel
 * has a knob for every aux: a channel that does not send to the aux yet
 * shows it at silence, and the first movement makes the send as part of
 * that same undo step (undo takes the send away again). Double-click: back
 * to silence, the default. Right-click: pre-fader, mute, remove.
 *
 * Undo: a drag is one step from the values before to the values after (the
 * level moves live meanwhile), the toggles and the removal are one step each.
 * A click that moves nothing is no step and leaves no send behind.
 */
class SendKnobComponent : public juce::Component,
                          public juce::SettableTooltipClient
{
public:
    static constexpr int height = MixerLayout::knobNameHeight + MixerLayout::knobHeight;   ///< the name over the dial
    static constexpr double maximumDecibels = 6.0;   ///< a send's top; the fader itself goes to +12

    SendKnobComponent (MixerContext context_, int trackId_, int channelNumber_, int auxId_, const juce::String& auxName_) :
        context (std::move (context_)),
        trackId (trackId_),
        channelNumber (channelNumber_),
        auxId (auxId_),
        auxName (auxName_),
        knob (*this)
    {
        AudiumLookAndFeel::configureFaderSlider (&knob);   // the taper, the dB text
        knob.setNormalisableRange (sendRange());           // ... cut off at +6 dB
        knob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        knob.setRotaryParameters (AudiumLookAndFeel::dialStartAngle, AudiumLookAndFeel::dialEndAngle, true);
        knob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        knob.setMouseDragSensitivity (120);
        knob.setDoubleClickReturnValue (false, 0.0);   // the double-click reset goes through performChange, see Knob
        knob.getProperties().set (AudiumLookAndFeel::dialArcFromProperty, 0.0);
        knob.getProperties().set (AudiumLookAndFeel::dialMarkProperty, 0.0);   // the mark at the default, silence
        knob.onDragStart = [this] { dragStarted(); };
        knob.onValueChange = [this] { dragged(); };
        knob.onDragEnd = [this] { dragEnded(); };
        addAndMakeVisible (knob);

        setTooltip (TRANS ("Send to ") + auxName);
        refresh();
    }

    int getAuxId() const noexcept { return auxId; }

    void setAuxName (const juce::String& newName)
    {
        if (auxName == newName)
            return;
        auxName = newName;
        setTooltip (TRANS ("Send to ") + auxName);
        repaint();
    }

    /// Re-reads the send's values; a drag in progress keeps its level.
    void refresh()
    {
        if (dragging)
            return;
        auto state = sendState();
        shown = state != nullptr ? audium::UndoableSendAction::Values::of (*state) : audium::UndoableSendAction::Values();

        knob.setValue (decibels(), juce::dontSendNotification);
        knob.textFromValueFunction = [pre = shown.preFader] (double value)
        {
            return AudiumLookAndFeel::decibelsText (value) + (pre ? " pre" : "");
        };
        knob.updateText();
        if (shown.mute)
            knob.setColour (audium::panIndicatorColourId, findColour (audium::muteColourId));
        else
            knob.removeColour (audium::panIndicatorColourId);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (findColour (audium::defaultTextColourId).withAlpha (0.85f));
        g.setFont (juce::FontOptions (8.0f));
        g.drawText (auxName, getLocalBounds().removeFromTop (MixerLayout::knobNameHeight), juce::Justification::centred, true);
    }

    void resized() override
    {
        knob.setBounds (getLocalBounds().withTrimmedTop (MixerLayout::knobNameHeight));
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
            showMenu();
    }

private:
    /// The slider: a right-click opens the send's menu, a double-click resets to silence as an undo step.
    class Knob : public juce::Slider
    {
    public:
        explicit Knob (SendKnobComponent& owner_) : owner (owner_) {}

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu())
                owner.showMenu();
            else
                juce::Slider::mouseDown (e);
        }

        void mouseDoubleClick (const juce::MouseEvent&) override { owner.resetToSilence(); }

    private:
        SendKnobComponent& owner;
    };

    std::shared_ptr<audium::AudioChannel> channel() const
    {
        auto track = context.container().getAudioTrack (trackId);
        return track != nullptr ? track->getChannel (channelNumber) : nullptr;
    }

    std::shared_ptr<audium::SendState> sendState() const
    {
        auto ch = channel();
        return ch != nullptr ? ch->sends.get (auxId) : nullptr;
    }

    /// The fader's taper over -80 .. +6 dB: the fader's travel up to +6 stretched over the whole sweep.
    static juce::NormalisableRange<double> sendRange()
    {
        using Taper = AudiumLookAndFeel::FaderTaper;
        const auto top = Taper::positionFor (maximumDecibels);
        return { Taper::minimumDecibels, maximumDecibels,
                 [top] (double, double, double position) { return Taper::decibelsFor (position * top); },
                 [top] (double, double, double decibels) { return Taper::positionFor (decibels) / top; } };
    }

    static float gainFor (double decibels) noexcept
    {
        return decibels <= AudiumLookAndFeel::FaderTaper::minimumDecibels ? 0.0f : juce::Decibels::decibelsToGain (static_cast<float> (decibels));
    }

    double decibels() const noexcept
    {
        return shown.level <= 0.0f ? AudiumLookAndFeel::FaderTaper::minimumDecibels : static_cast<double> (LevelMeter::gainToDecebel (shown.level));
    }

    /// The send, made at silence if the channel has none into this aux yet: the undo step the caller
    /// performs next carries the making (createdInThisStep), so undo removes the entry again.
    std::shared_ptr<audium::SendState> sendStateOrCreate()
    {
        if (auto state = sendState())
            return state;
        auto ch = channel();
        if (ch == nullptr)
            return nullptr;
        auto made = ch->sends.getOrAdd (auxId);
        if (made != nullptr)
            createdInThisStep = true;
        return made;
    }

    /// A gesture that changed nothing records no step, so an entry it made would stay as a silent leftover: it goes again.
    void dropEntryMadeInThisStep()
    {
        if (! std::exchange (createdInThisStep, false))
            return;
        if (auto ch = channel())
            ch->sends.remove (auxId);
    }

    /// The slider's mouse-down: a click without movement comes through here too, so nothing is made yet.
    void dragStarted()
    {
        dragging = true;
        auto state = sendState();
        valuesAtDragStart = state != nullptr ? audium::UndoableSendAction::Values::of (*state) : audium::UndoableSendAction::Values();   // no send yet: silence
    }

    void dragged()
    {
        if (! dragging)
            return;
        if (auto state = sendStateOrCreate()) {   // the first movement makes the send
            shown.level = gainFor (knob.getValue());
            state->level.store (shown.level, std::memory_order_relaxed);   // live; the step comes at the drag's end
        }
    }

    void dragEnded()
    {
        if (! dragging)
            return;
        dragging = false;
        auto state = sendState();
        if (state == nullptr)
            return;   // nothing was moved, or the channel is gone
        const auto newValues = audium::UndoableSendAction::Values::of (*state);
        if (newValues == valuesAtDragStart) {
            dropEntryMadeInThisStep();   // back where it started: no step
            refresh();
            return;
        }
        // the send holds the new values already: perform() stores them again, a no-op, rather than this
        // writing the old ones back first and handing the audio thread a block at the old level in between
        performChange (valuesAtDragStart, newValues);
    }

    /// The default, -inf: a send that is there goes silent as one undo step; one that is not stays as it is.
    void resetToSilence()
    {
        // a double-click's second mouse-down opened a drag bracket: it closes here without a step of its own, the
        // reset is the one step, from the values before that click
        const auto wasDragging = std::exchange (dragging, false);
        auto state = sendState();
        if (state == nullptr)
            return;
        const auto oldValues = wasDragging ? valuesAtDragStart : audium::UndoableSendAction::Values::of (*state);
        if (oldValues.level <= 0.0f) {
            dropEntryMadeInThisStep();
            refresh();
            return;
        }
        auto values = oldValues;
        values.level = 0.0f;
        performChange (oldValues, values);
    }

    void performChange (const audium::UndoableSendAction::Values& oldValues, const audium::UndoableSendAction::Values& newValues)
    {
        const auto created = std::exchange (createdInThisStep, false);
        context.undoManager().beginNewTransaction();
        context.undoManager().perform (new audium::UndoableSendAction (context.container(), trackId, channelNumber, auxId, oldValues, newValues, created),
                                       "Set Send");
        refresh();
    }

    void showMenu()
    {
        auto state = sendState();
        juce::PopupMenu menu;
        const auto values = state != nullptr ? audium::UndoableSendAction::Values::of (*state) : audium::UndoableSendAction::Values();
        menu.addItem (TRANS ("Pre-Fader"), state != nullptr, values.preFader, [this, values]
        {
            if (auto s = sendState()) {
                auto changed = values;
                changed.preFader = ! values.preFader;
                performChange (audium::UndoableSendAction::Values::of (*s), changed);
            }
        });
        menu.addItem (TRANS ("Mute"), state != nullptr, values.mute, [this, values]
        {
            if (auto s = sendState()) {
                auto changed = values;
                changed.mute = ! values.mute;
                performChange (audium::UndoableSendAction::Values::of (*s), changed);
            }
        });
        menu.addSeparator();
        menu.addItem (TRANS ("Remove"), state != nullptr, false, [this]
        {
            auto ch = channel();
            if (ch == nullptr)
                return;
            const auto id = auxId;
            audium::applyAsUndoableEdit (context.container(), [&] { return ch->sends.remove (id); }, "Remove Send", false);   // the knobs re-read on its broadcast
        });
        menu.showMenuAsync (MixerContext::menuOptions (this));
    }

    MixerContext context;
    const int trackId;
    const int channelNumber;
    const int auxId;
    juce::String auxName;
    Knob knob;

    audium::UndoableSendAction::Values shown;
    bool dragging = false;
    bool createdInThisStep = false;
    audium::UndoableSendAction::Values valuesAtDragStart;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SendKnobComponent)
};
