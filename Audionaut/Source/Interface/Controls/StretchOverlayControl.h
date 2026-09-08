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
 * this clip. The row is Mode, Lock, a slider, and Apply:
 *
 * - Unlocked, the slider is the speed ratio (0.25x-4x, log around 1.0,
 *   double-click for x1) and applies LIVE - the clip stretches as the value
 *   moves.
 * - Lock ties the clip to the project tempo: its speed becomes project
 *   tempo / clip tempo and follows every tempo change, live during
 *   playback. The clip tempo is seeded from the beat analysis (queued if
 *   it is missing, adopted when it arrives) and the slider then edits that
 *   tempo in BPM, with a /2 and x2 pair for the beat tracker's octave
 *   errors; double-click returns to the detected tempo.
 *
 * The Mode box picks how the speed is realised: Re-Pitch is classic
 * varispeed, Time-Stretch keeps the pitch (see StretchAudioSource). Apply
 * keeps the session as one "Stretch Clip" undo transaction and closes;
 * Escape and the close chip roll the whole session back instead. Moving
 * the selection to another clip, or toggling the command off, keeps the
 * changes (committing the session) - only the two explicit dismiss
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

    // Follows the selection to another clip while shown, and adopts a beat
    // analysis that finishes while a tempo-locked clip still has no tempo.
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

protected:
    int getPreferredWidth() const override;

    // When space is tight the Mode box, Lock and the /2 x2 pair sit out
    // first: a slider-plus-Apply control can still finish the session.
    int getMinimumWidth() const override;

    // Escape / the close chip CANCEL: the pending session is rolled back
    // (no undo entry) before the cleared target hides this control.
    void dismissOverlay() override;

    // Session begin: arm a clean session, sync the widgets from the clip,
    // pick up a detection that finished while the overlay was away.
    void overlayShown() override;

    // Session end: commit whatever is still pending as one undo step -
    // this is what keeps the changes on selection moves and the command
    // toggling off. After Apply or a cancel the session is clean and this
    // is a no-op.
    void overlayHidden() override;

    // Someone else changed the arrangement (a Cmd+Alt stretch drag, undo)
    // or the tempo moved under a locked clip; keep the readout honest -
    // but never mid-drag on our own slider.
    void overlayActionReceived (const juce::String& message) override;

    // The base's button labels plus "Speed"/"Tempo" under the slider and
    // "Mode" under the box.
    void paintLabels (juce::Graphics& g) override;

private:
    // The live-apply paths: each opens the undo session on the first
    // change, edits the clip, reflects the clamped value back into the
    // slider and relays the arrangement out.
    void applyRatio (double newRatio);
    void applyClipTempo (double bpm);
    void applyTempoLock (bool shouldLock);

    // Same session plumbing for the Mode box (see class comment).
    void applyMode (bool pitchPreserving);

    // A locked clip without a tempo takes the detected one once it exists.
    void adoptDetectedTempo();

    // Opens the one-per-session undo action lazily; all apply paths share it.
    void ensureSessionOpen (audium::PlayListItem& item);

    // Commits the pending session as one "Stretch Clip" undo transaction.
    // Idempotent; called from Apply, the hidden hook and the destructor (a
    // rebuild destroys visible overlays without a visibilityChanged).
    void commitSession();

    // Rolls the pending session back to its opening state, leaving no undo
    // entry. Idempotent; called from the dismiss gestures.
    void cancelSession();

    // The current target item, re-resolved from the engine.
    std::shared_ptr<audium::PlayListItem> resolveItem() const;

    // Reads the item's lock state, speed / tempo and mode into the widgets
    // without notifying.
    void syncFromEngine();

    // Swaps the slider between the ratio and the BPM configuration.
    void configureSlider (bool locked);

    // True once the session opened an undo action (first value change).
    bool sessionDirty = false;

    // Which configuration the slider currently has (see configureSlider).
    bool sliderShowsTempo = false;

    std::unique_ptr<juce::Slider> speedSlider;
    std::unique_ptr<juce::DrawableButton> lockButton;
    std::unique_ptr<juce::DrawableButton> halveButton, doubleButton;
    std::unique_ptr<juce::ComboBox> modeBox;
    std::unique_ptr<juce::DrawableButton> applyButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StretchOverlayControl)
};
