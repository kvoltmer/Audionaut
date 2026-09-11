//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <memory>
#include <JuceHeader.h>

#include "Engine/AutoEdit/AutoEdit.h"
#include "Interface/Controls/ClipOverlayBase.h"

/**
 * @class AutoEditOverlayControl
 * @brief In-arrangement control for a pending auto edit.
 *
 * Replaces the old Auto Edit dialog: PlayListItemComponent shows it on the
 * clip whose merge preview is active, floating above AutoEditPreviewView's
 * dashed cuts. It offers only the abstract measure parameter - stepped
 * musically by a Less/More button pair speaking in segments: More cuts more
 * (halving the measures), Less cuts fewer (doubling them) - and an Apply
 * button. The preview underneath is the feedback; the resulting measures
 * value is also logged to the console.
 *
 * While shown, the control follows the selection: selecting another clip
 * re-publishes the preview there, keeping the measures value - which
 * travels with the preview (AnalysisProvider::getMergePreviewMeasures), so
 * the clip now showing the control picks it up.
 *
 * Escape cancels the pending edit (clearing the preview hides the control);
 * so does invoking the Auto Edit command again. Positioning, zoom fading
 * and the RegionSelector takeover come from ClipOverlayBase.
 */
class AutoEditOverlayControl : public ClipOverlayBase
{
public:
    /// The measures value a fresh preview starts from, here and in the
    /// command that publishes it (see AudiumMainWindow).
    static constexpr double defaultMeasures = 4.0;

    AutoEditOverlayControl(std::shared_ptr<audium::AudiumEngine> audiumEngine,
                           std::shared_ptr<RegionSelector> regionSelector);

    void resized() override;

    // Follows the selection to another clip (see class comment).
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

protected:
    int getPreferredWidth() const override;

    // When space is tight the Less/More pair sits out first: an Apply-only
    // control can still finish the edit.
    int getMinimumWidth() const override;

    // Cancels the pending edit: clearing the preview hides the control.
    void dismissOverlay() override;

    // The preview may have moved here from another clip; adopt its
    // measures value and refresh the buttons and the Xfade default.
    void overlayShown() override;

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

    // The abstract parameter's value, in measures (see AutoEditParameter).
    double measures = defaultMeasures;

    std::unique_ptr<juce::DrawableButton> lessButton, moreButton;
    std::unique_ptr<juce::DrawableButton> xfadeButton;
    std::unique_ptr<juce::DrawableButton> applyButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoEditOverlayControl)
};
