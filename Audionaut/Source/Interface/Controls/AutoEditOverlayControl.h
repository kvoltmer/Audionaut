//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <memory>
#include <JuceHeader.h>

#include "Engine/AudiumEngine.h"
#include "Engine/AutoEdit/AutoEdit.h"

class RegionSelector;

/**
 * @class AutoEditOverlayControl
 * @brief Transparent in-arrangement control for a pending auto edit.
 *
 * Replaces the old Auto Edit dialog: PlayListItemComponent shows it on the
 * clip whose merge preview is active, floating above AutoEditPreviewView's
 * dashed cuts. It offers only the abstract measure parameter - stepped
 * musically by a Less/More button pair speaking in segments: More cuts more
 * (halving the measures), Less cuts fewer (doubling them) - and an Apply
 * button. The preview underneath is the feedback; the resulting measures
 * value is also logged to the console.
 *
 * The control keeps itself centred vertically over the clip and horizontally
 * within the part of the clip visible in the enclosing viewport, so it stays
 * in reach however far the arrangement is scrolled or zoomed. It re-checks
 * its spot when the arrangement says something moved - the scroll broadcast
 * every viewport change already triggers (audium::scrolledVertically) - and
 * glides there through the shared ComponentAnimator.
 *
 * Zooming re-lays the clip out continuously (parentSizeChanged); rather than
 * chasing that, the control fades out and comes back at the fresh spot once
 * the zoom has been quiet for a moment. The only timer here is the one-shot
 * delay for that reappearance - position updates are all event-driven.
 *
 * While shown, the control follows the selection: selecting another clip
 * re-publishes the preview there, keeping the measures value - which travels
 * with the preview (AnalysisProvider::getMergePreviewMeasures), so the clip
 * now showing the control picks it up.
 *
 * Escape cancels the pending edit (clearing the preview hides the control);
 * so does invoking the Auto Edit command again.
 *
 * The RegionSelector listens to the whole arrangement, so it is disabled for
 * as long as the pending edit is active - not just under the cursor, as the
 * hover controls (FadeInOutControl, DraggerControl) do - and handed back when
 * the edit is applied or cancelled. While an auto edit is pending, the
 * arrangement is in that edit's mode and nothing else's. Those hover controls
 * hand the selector back on their own mouseExit, though, which would leave it
 * armed under this control's buttons - so entering this control (or its
 * buttons) re-asserts the takeover (see mouseEnter).
 */
class AutoEditOverlayControl : public juce::Component,
                               public juce::ChangeListener,
                               private juce::ActionListener,
                               private juce::Timer
{
public:
    /// The measures value a fresh preview starts from, here and in the
    /// command that publishes it (see AudiumMainWindow).
    static constexpr double defaultMeasures = 4.0;

    AutoEditOverlayControl(std::shared_ptr<audium::AudiumEngine> audiumEngine,
                           std::shared_ptr<RegionSelector> regionSelector);
    ~AutoEditOverlayControl() override;

    /** @brief Names the clip whose edit this control drives. */
    void setTarget(int trackId, int playlistItemId);

    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;
    bool keyPressed (const juce::KeyPress& key) override;

    // A hover control's mouseExit may have re-enabled the region selector
    // while the edit is pending; entering here disables it again before a
    // button click could start a selection. Received for the child buttons
    // too - the constructor registers for nested mouse events. There is no
    // re-enabling mouseExit: the selector stays off until the control hides
    // (see visibilityChanged), as the class comment describes.
    void mouseEnter (const juce::MouseEvent& e) override;

    // A zoom re-laid the clip out under the control: fade away and schedule
    // the comeback.
    void parentSizeChanged() override;

    // Follows the selection to another clip (see class comment).
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

private:
    audium::AutoEditConfig makeConfig() const;

    // Doubles (longer = true) or halves the measures, clamped to [1, 64],
    // logs the result and refreshes the preview. Longer segments mean fewer
    // of them - the Less button - and vice versa.
    void stepMeasures(bool longer);

    // Whether a preview at this measures value would still cut the clip.
    // Segments longer than the clip leave no boundary inside it; publishing
    // that empty preview hides this control, cancelling the edit - so Less
    // must stop one step before it (see updateStepButtons).
    bool wouldCut(double steppedMeasures) const;

    // A button whose step could go no further is disabled.
    void updateStepButtons();

    // Re-publishes the preview for the current measures value.
    void updatePreview();

    // Applies the edit. Deferred, because replacing the clip destroys the
    // component hierarchy this control lives in - including the button whose
    // click got us here.
    void apply();

    // The arrangement scrolled (audium::scrolledVertically fires on every
    // viewport change, horizontal included).
    void actionListenerCallback (const juce::String& message) override;

    // Centres the control in the visible part of the clip, animating there;
    // the first update after showing jumps straight to the spot.
    void updatePosition();

    // Fades the control away while a zoom reshapes the clip; every further
    // zoom notification pushes the comeback out again.
    void hideForZoom();

    // The one-shot comeback: reappear at the fresh spot.
    void timerCallback() override;

    // Re-enables the region selector, which stays off while this control is
    // shown - i.e. while the auto edit is pending.
    void releaseRegionSelector();

    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    std::shared_ptr<RegionSelector> regionSelector;

    int trackId = -1;
    int playlistItemId = -1;

    // Where the control is headed, in viewport coordinates. Stillness means
    // holding a position on screen - in clip coordinates the clip itself
    // moves with every scroll and zoom, so anchoring there would wobble.
    // The destination is only re-committed once the on-screen ideal has
    // drifted a real distance; inside that dead zone the control re-pins
    // itself to the committed screen spot instead.
    juce::Rectangle<int> committedTarget;

    bool snapOnNextUpdate = true;

    // Faded away while a zoom is reshaping the clip (see hideForZoom).
    bool hiddenForZoom = false;

    // The abstract parameter's value, in measures (see AutoEditParameter).
    double measures = defaultMeasures;

    std::unique_ptr<juce::DrawableButton> lessButton, moreButton;
    std::unique_ptr<juce::DrawableButton> applyButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoEditOverlayControl)
};
