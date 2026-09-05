//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <memory>
#include <JuceHeader.h>

#include "Interface/Controls/ClipOverlayBase.h"

/**
 * @class StretchOverlayControl
 * @brief In-arrangement editor for a clip's playback speed.
 *
 * The Stretch Clip command mounts it on the selected clip:
 * PlayListItemComponent shows it while the engine's ClipOverlayTarget names
 * this clip. A ratio slider (0.25x-4x, log around 1.0), a semitone step
 * pair and a x1 reset all apply LIVE - the clip stretches as the value
 * moves. Apply keeps the session as one "Stretch Clip" undo transaction and
 * closes; Escape and the close chip roll the whole session back instead.
 * Moving the selection to another clip, or toggling the command off, keeps
 * the changes (committing the session) - only the two explicit dismiss
 * gestures on the panel cancel.
 *
 * Positioning, zoom fading, the RegionSelector takeover and the Escape
 * handling come from ClipOverlayBase.
 */
class StretchOverlayControl : public ClipOverlayBase
{
public:
    StretchOverlayControl(std::shared_ptr<audium::AudiumEngine> audiumEngine,
                          std::shared_ptr<RegionSelector> regionSelector);
    ~StretchOverlayControl() override;

    void resized() override;

    // Follows the selection to another clip while shown.
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

protected:
    int getPreferredWidth() const override;

    // When space is tight the steppers and reset sit out first: a
    // slider-plus-Apply control can still finish the session.
    int getMinimumWidth() const override;

    // Escape / the close chip CANCEL: the pending session is rolled back
    // (no undo entry) before the cleared target hides this control.
    void dismissOverlay() override;

    // Session begin: sync the slider from the clip, arm a clean session.
    void overlayShown() override;

    // Session end: commit whatever is still pending as one undo step -
    // this is what keeps the changes on selection moves and the command
    // toggling off. After Apply or a cancel the session is clean and this
    // is a no-op.
    void overlayHidden() override;

    // Someone else changed the arrangement (a Cmd+Alt stretch drag, undo);
    // keep the readout honest - but never mid-drag on our own slider.
    void overlayActionReceived (const juce::String& message) override;

    // The base's button labels plus "Speed" under the slider.
    void paintLabels (juce::Graphics& g) override;

private:
    // The one live-apply path: opens the undo session on the first change,
    // sets the clip's speed, reflects the clamped value back into the
    // slider and relays the arrangement out.
    void applyRatio(double newRatio);

    // Commits the pending session as one "Stretch Clip" undo transaction.
    // Idempotent; called from Apply, the hidden hook and the destructor (a
    // rebuild destroys visible overlays without a visibilityChanged).
    void commitSession();

    // Rolls the pending session back to its opening state, leaving no undo
    // entry. Idempotent; called from the dismiss gestures.
    void cancelSession();

    // The current target item, re-resolved from the engine.
    std::shared_ptr<audium::PlayListItem> resolveItem() const;

    // Reads the item's speed into the slider without notifying.
    void syncFromEngine();

    // True once the session opened an undo action (first value change).
    bool sessionDirty = false;

    std::unique_ptr<juce::Slider> speedSlider;
    std::unique_ptr<juce::DrawableButton> semitoneDownButton, semitoneUpButton;
    std::unique_ptr<juce::DrawableButton> resetButton;
    std::unique_ptr<juce::DrawableButton> applyButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StretchOverlayControl)
};
