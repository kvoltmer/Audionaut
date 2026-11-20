//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Engine/Effects/Hosting/HostedEffect.h"
#include "Interface/Components/MixerPanel/MixerContext.h"
#include "Interface/Components/MixerPanel/SlotEditor.h"

/**
 * The editor window content of a hosted plugin: the plugin's own editor,
 * nothing around it. A plugin without one gets JUCE's generic parameter
 * list; a placeholder (the plugin failed to load) gets the reason as text.
 * A plugin that is there but refuses the chain's channel count keeps its
 * editor, with a strip over it that says the signal passes through and why.
 * Bypass is the slot's (its menu in the mixer); a bypassed slot's editor
 * is dimmed.
 *
 * The component takes the size the plugin's editor wants and follows when
 * the editor changes it (the window follows the component); a plugin whose
 * editor lets the user resize it hands the window's room on to it.
 *
 * What the user does in the plugin's window becomes an undo step, as a
 * dial of a built-in's editor does: the plugin reports its changes, and
 * once a move is over (the control released, no report for a moment) the
 * state before and after go to the undo manager - so the project knows it
 * changed and asks to be saved. A state the host itself put into the plugin
 * (an undo, an agent's edit) is not a move of the user's: the reports that
 * follow it are let pass, and the state they leave is the next start.
 * Neither wait is open-ended: a plugin that reports without pause, or that
 * leaves a gesture open, gets its step after a few seconds all the same.
 *
 * The slot is held strongly: the plugin instance outlives its editor, which
 * the destructor takes down first. Whether the slot is still in the project
 * is polled on a timer; once it is not, the owner is asked to close the
 * window.
 */
class HostedEffectEditorComponent : public juce::Component,
                                    public SlotEditor,
                                    private juce::Timer
{
public:
    static constexpr int minimumWidth = 200;
    static constexpr int messageWidth = 300;    ///< a placeholder's text box
    static constexpr int messageHeight = 90;
    static constexpr int margin = 10;
    static constexpr int warningStripHeight = 28;   ///< over the editor of a plugin that does not run

    HostedEffectEditorComponent (MixerContext context, audium::InsertChainAddress address, std::shared_ptr<audium::EffectSlot> slot);
    ~HostedEffectEditorComponent() override;

    bool isStillValid() const override;

    /// Whether the plugin's editor lets the user resize it; the window is then resizable too.
    bool isUserResizable() const;

    struct SizeLimits { int minimumWidth, minimumHeight, maximumWidth, maximumHeight; };
    /// The limits the plugin's editor sets: the window's resize limits.
    SizeLimits getSizeLimits() const;

    void resized() override;
    void paint (juce::Graphics& g) override;
    void childBoundsChanged (juce::Component* child) override;

private:
    void timerCallback() override;
    void refresh();
    int slotIndex() const;
    void createEditor();
    void deleteEditor();
    void followEditorSize();
    int warningHeight() const;
    void startTrackingEdits();
    void trackEdits();
    void commitEdit();

    static constexpr juce::uint32 quietMilliseconds = 500;   ///< no report for this long ends a move
    static constexpr juce::uint32 longestRunMilliseconds = 3000;    ///< reports without pause, no control held: a step is taken after this long anyway
    static constexpr juce::uint32 staleGestureMilliseconds = 2000;  ///< a control reported as held, with no report for this long, counts as released

    MixerContext context;
    audium::InsertChainAddress address;
    std::shared_ptr<audium::EffectSlot> slot;   ///< strong: the plugin lives as long as its editor
    audium::HostedEffect& hosted;

    /// Keeps a component's own painting off the screen, so what is behind it shows. For an Audio Unit's
    /// editor component, which paints itself white under the plugin's native view: a view that draws no
    /// background (the system's generic one, some of Apple's own) takes its colours from the window's dark
    /// appearance, and its white labels on that white could not be read. Without it the window's own
    /// background is behind the view. The native view is no part of the painting and stays as it is.
    struct PaintsNothing final : public juce::ImageEffectFilter
    {
        void applyEffect (juce::Image&, juce::Graphics&, float, float) override {}
    };
    PaintsNothing paintsNothing;

    std::unique_ptr<juce::AudioProcessorEditor> editor;
    bool following = false;   ///< sizing this to the editor right now: its bounds are not to be written back
    audium::HostedEffect::Status shownStatus = audium::HostedEffect::Status::loaded;   ///< what the placeholder's text was painted for

    // the user's moves in the plugin's window, as undo steps
    nlohmann::json baseline;                ///< the state the next step starts from
    juce::uint32 seenPluginChanges = 0;
    juce::uint32 seenHostEdits = 0;
    juce::uint32 waitingSince = 0;         ///< when the pending move, or the settling, began
    juce::uint32 lastChange = 0;           ///< when the plugin last reported one, or the host last edited
    bool tracking = false;                 ///< the plugin is there
    bool editPending = false;              ///< the plugin reported changes that are not a step yet
    bool settling = false;                 ///< the reports are the echo of a host edit

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HostedEffectEditorComponent)
};
