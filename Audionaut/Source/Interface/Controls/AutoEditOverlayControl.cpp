//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <cmath>
#include <iostream>

#include "AutoEditOverlayControl.h"
#include "Engine/ActionMessages.h"
#include "Engine/Analysis/AnalysisProvider.h"
#include "Engine/Selection/SelectionManager.h"
#include "Interface/Controls/RegionSelector.h"

namespace {

// A row of icon buttons sized like the header's transport buttons
// (see HeaderComponent::resized), with their text labels drawn underneath on
// the scrim (see paint()).
constexpr int buttonWidth = 35;
constexpr int buttonHeight = 20;
constexpr int labelHeight = 12;
constexpr float labelFontHeight = 10.0f;
constexpr int controlHeight = 3 + buttonHeight + labelHeight + 3;
constexpr int stepGap = 8;
constexpr int gap = 12;
constexpr int padding = 6;

// The measure parameter's range. Stepping halves or doubles, so every
// reachable value is a musically meaningful power of two.
constexpr double minMeasures = 1.0;
constexpr double maxMeasures = 64.0;

// How far the control keeps clear of the visible edges, so it sits well
// within the arrangement rather than hugging a border.
constexpr int visibleMargin = 16;

// The dead zone: how far the ideal spot must drift before the control goes
// after it. Scrolling via the zoom control moves the visible range by a few
// pixels per notification, and chasing every nudge reads as jitter - sitting
// still until the drift is real does not.
constexpr int repositionDistance = 100;

// How long the glide to a re-committed spot takes. Unhurried on purpose: the
// dead zone is small, so the control corrects often and each correction
// should read as a drift, not a hop.
constexpr int glideMilliseconds = 1000;

// The zoom fade: how quickly the control gets out of the way, and how long
// the zoom must be quiet before it comes back.
constexpr int fadeMilliseconds = 50;
constexpr int comebackDelayMilliseconds = 500;

constexpr int preferredWidth = padding + buttonWidth + stepGap + buttonWidth
                                   + gap + buttonWidth + padding;

// When space is tight the Less/More pair sits out first: an Apply-only
// control can still finish the edit.
constexpr int applyOnlyWidth = padding + buttonWidth + padding;

/**
 * Icon paths, all in a shared 24-unit design box so the three icons scale
 * identically. DrawableButton fits the drawable's own bounds into the image
 * area, so the invisible corner subpaths pin every path's bounds to the full
 * box - without them a lone minus bar would be blown up to fill the area.
 */
void pinToDesignBox (juce::Path& path)
{
    path.startNewSubPath (0.0f, 0.0f);
    path.startNewSubPath (24.0f, 24.0f);
}

// Less: minus - fewer, longer segments.
juce::Path minusIconPath()
{
    juce::Path path;
    path.addRoundedRectangle (3.0f, 10.75f, 18.0f, 2.5f, 1.25f);
    pinToDesignBox (path);
    return path;
}

// More: plus - more, shorter segments.
juce::Path plusIconPath()
{
    juce::Path path;
    path.addRoundedRectangle (3.0f, 10.75f, 18.0f, 2.5f, 1.25f);
    path.addRoundedRectangle (10.75f, 3.0f, 2.5f, 18.0f, 1.25f);
    pinToDesignBox (path);
    return path;
}

// Apply: check mark.
juce::Path checkIconPath()
{
    juce::Path line;
    line.startNewSubPath (4.5f, 13.0f);
    line.lineTo (10.0f, 18.5f);
    line.lineTo (19.5f, 6.0f);

    juce::Path path;
    juce::PathStrokeType (2.5f, juce::PathStrokeType::curved,
                          juce::PathStrokeType::rounded).createStrokedPath (path, line);
    pinToDesignBox (path);
    return path;
}

/**
 * An icon button looking like the header's transport buttons - same style
 * and the stop button's grey. The label is not part of the button - paint()
 * draws it underneath on the scrim. The disabled image has to be handed over
 * explicitly: the look-and-feel dims only the background of a disabled
 * button, not its drawable.
 */
std::unique_ptr<juce::DrawableButton> makeIconButton (const juce::String& label,
                                                      const juce::Path& iconPath)
{
    auto button = std::make_unique<juce::DrawableButton> (
        label, juce::DrawableButton::ButtonStyle::ImageOnButtonBackground);

    button->setColour (juce::TextButton::buttonColourId, juce::Colours::grey);

    juce::DrawablePath icon;
    icon.setPath (iconPath);
    icon.setFill (juce::FillType (juce::Colours::white));

    juce::DrawablePath disabledIcon;
    disabledIcon.setPath (iconPath);
    disabledIcon.setFill (juce::FillType (juce::Colours::white.withAlpha (0.4f)));

    // setImages() deep-copies, so the stack locals are safe to hand over.
    button->setImages (&icon, nullptr, nullptr, &disabledIcon);

    return button;
}

} // namespace

AutoEditOverlayControl::AutoEditOverlayControl(std::shared_ptr<audium::AudiumEngine> audiumEngine_,
                                               std::shared_ptr<RegionSelector> regionSelector_) :
    audiumEngine(audiumEngine_),
    regionSelector(regionSelector_)
{
    setWantsKeyboardFocus (true);

    // More/less speak in segments: more segments means shorter ones, so More
    // halves the measures and Less doubles them - hence minus for Less and
    // plus for More.
    lessButton = makeIconButton (TRANS ("Less"), minusIconPath());
    addAndMakeVisible (lessButton.get());
    lessButton->onClick = [this]() {
        stepMeasures(true);
    };

    moreButton = makeIconButton (TRANS ("More"), plusIconPath());
    addAndMakeVisible (moreButton.get());
    moreButton->onClick = [this]() {
        stepMeasures(false);
    };

    applyButton = makeIconButton (TRANS ("Apply"), checkIconPath());
    addAndMakeVisible (applyButton.get());
    applyButton->onClick = [this]() {
        apply();
    };

    // The mouse spends its time here over the buttons, and mouseEnter has to
    // fire for those too (see the header).
    addMouseListener (this, true);

    updateStepButtons();
}

AutoEditOverlayControl::~AutoEditOverlayControl()
{
    audiumEngine->getAudioTrackContainer()->removeActionListener(this);
    audiumEngine->getAudioTrackContainer()->getSelectionManager()->removeChangeListener(this);
    releaseRegionSelector();
}

void AutoEditOverlayControl::releaseRegionSelector()
{
    if (regionSelector != nullptr)
        regionSelector->setEnabled(true);
}

void AutoEditOverlayControl::setTarget(int trackId_, int playlistItemId_)
{
    trackId = trackId_;
    playlistItemId = playlistItemId_;
}

audium::AutoEditConfig AutoEditOverlayControl::makeConfig() const
{
    audium::AutoEditConfig config;
    config.trackId = trackId;
    config.playlistItemId = playlistItemId;
    config.segmentMeasures = measures;

    return config;
}

void AutoEditOverlayControl::stepMeasures(bool longer)
{
    const auto stepped = juce::jlimit (minMeasures, maxMeasures,
                                       longer ? measures * 2.0 : measures / 2.0);

    // The Less button is already disabled one step before the preview would
    // go empty (see wouldCut); this guard keeps the invariant if a step
    // arrives some other way.
    if (longer && ! wouldCut (stepped))
        return;

    measures = stepped;

    std::cout << "AutoEdit measures: " << measures << std::endl;

    updateStepButtons();
    updatePreview();
}

bool AutoEditOverlayControl::wouldCut(double steppedMeasures) const
{
    audium::AutoEdit autoEdit(audiumEngine);

    auto config = makeConfig();
    config.segmentMeasures = steppedMeasures;

    // Zero means no boundary falls inside the clip; an unresolved target or
    // missing analyses report a nonzero placeholder, so only a definite
    // zero-cut step is refused.
    return autoEdit.resolveNumSegments(config) != 0;
}

void AutoEditOverlayControl::updateStepButtons()
{
    // Less doubles the measures, More halves them (see the constructor).
    // Less also stops where doubling would leave the clip uncut - previewing
    // zero cuts would hide this control and cancel the edit.
    lessButton->setEnabled (measures < maxMeasures && wouldCut (measures * 2.0));
    moreButton->setEnabled (measures > minMeasures);

    // The labels live in this component's paint() and dim with their button.
    repaint();
}

void AutoEditOverlayControl::updatePreview()
{
    audium::AutoEdit autoEdit(audiumEngine);
    auto config = makeConfig();
    autoEdit.previewAutoEdit(config);
}

void AutoEditOverlayControl::apply()
{
    juce::MessageManager::callAsync ([engine = audiumEngine, config = makeConfig()]() mutable {
        audium::AutoEdit autoEdit(engine);

        autoEdit.invokeAutoEdit(config, [](std::string error) {
#if CATCH2_TESTS
            std::cout << "error: " << error << std::endl;
#else
            juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                        "Error",
                                                        juce::String(error));
#endif
        });

        // The pending edit is over either way; the cleared preview hides
        // every overlay control.
        if (auto analysisProvider = engine->getAudioTrackContainer()->getAnalysisProvider())
            analysisProvider->clearMergePreview();
    });
}

void AutoEditOverlayControl::mouseEnter (const juce::MouseEvent&)
{
    // A hover control's mouseExit may have handed the selector back while the
    // edit is pending; the takeover is re-asserted the same way
    // visibilityChanged() established it, before a button click could start a
    // region selection instead.
    if (regionSelector != nullptr)
    {
        regionSelector->cancelSelection();
        regionSelector->setEnabled(false);
    }
}

bool AutoEditOverlayControl::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (auto analysisProvider = audiumEngine->getAudioTrackContainer()->getAnalysisProvider())
            analysisProvider->clearMergePreview();

        return true;
    }

    return false;
}

void AutoEditOverlayControl::paint (juce::Graphics& g)
{
    // A faint scrim, not a window: the waveform and the previewed cuts stay
    // readable underneath.
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 5.0f);

    // The labels sit below their buttons, on the scrim rather than inside the
    // button. Centred on the button, allowed to run wider than it - the
    // button spacing keeps neighbouring labels apart.
    g.setFont (juce::Font (juce::FontOptions (labelFontHeight)));

    for (auto* button : { lessButton.get(), moreButton.get(), applyButton.get() })
    {
        if (! button->isVisible())
            continue;

        const auto labelArea = button->getBounds()
                                   .withY (button->getBottom())
                                   .withHeight (labelHeight)
                                   .expanded (stepGap, 0);

        g.setColour (juce::Colours::white.withMultipliedAlpha (button->isEnabled() ? 1.0f : 0.4f));
        g.drawFittedText (button->getName(), labelArea, juce::Justification::centred, 1);
    }
}

void AutoEditOverlayControl::resized()
{
    auto r = getLocalBounds().reduced (padding, 3);

    // The buttons take the top strip; the strip below is where paint() puts
    // their labels.
    auto buttonRow = r.removeFromTop (buttonHeight);

    applyButton->setBounds (buttonRow.removeFromRight (buttonWidth));
    buttonRow.removeFromRight (gap);

    // At the Apply-only width (see updatePosition) the step pair sits out.
    const bool stepsFit = getWidth() >= preferredWidth;
    lessButton->setVisible (stepsFit);
    moreButton->setVisible (stepsFit);

    lessButton->setBounds (buttonRow.removeFromLeft (buttonWidth));
    buttonRow.removeFromLeft (stepGap);
    moreButton->setBounds (buttonRow.removeFromLeft (buttonWidth));
}

void AutoEditOverlayControl::visibilityChanged()
{
    auto audioTrackContainer = audiumEngine->getAudioTrackContainer();
    auto selectionManager = audioTrackContainer->getSelectionManager();

    if (isVisible())
    {
        // The preview may have moved here from another clip; its measures
        // value travels with it.
        if (auto analysisProvider = audiumEngine->getAudioTrackContainer()->getAnalysisProvider())
            if (analysisProvider->getMergePreviewMeasures() > 0.0)
                measures = analysisProvider->getMergePreviewMeasures();

        updateStepButtons();

        // Only the visible control follows the selection, so a pending edit
        // has exactly one follower.
        selectionManager->addChangeListener(this);

        // Auto edit takes over the arrangement: any region selection in
        // progress is dismissed, and none can start while the edit is
        // pending.
        if (regionSelector != nullptr)
        {
            regionSelector->cancelSelection();
            regionSelector->setEnabled(false);
        }

        // The arrangement announces every scroll on the track container;
        // that broadcast is what keeps the position fresh.
        audioTrackContainer->addActionListener(this);

        // A previous session may have ended mid-fade.
        hiddenForZoom = false;
        setInterceptsMouseClicks(true, true);
        setAlpha (1.0f);

        snapOnNextUpdate = true;
        updatePosition();

        if (isShowing())
            grabKeyboardFocus();
    }
    else
    {
        audioTrackContainer->removeActionListener(this);
        selectionManager->removeChangeListener(this);

        stopTimer();
        juce::Desktop::getInstance().getAnimator().cancelAnimation(this, false);

        // Applying or cancelling hides the control under the mouse, which
        // never delivers the mouseExit that would give the selector back.
        releaseRegionSelector();
    }
}

void AutoEditOverlayControl::actionListenerCallback (const juce::String& message)
{
    // While faded away for a zoom there is nothing to keep in place - the
    // comeback snaps to the fresh spot anyway.
    if (message == audium::scrolledVertically && ! hiddenForZoom)
        updatePosition();
}

void AutoEditOverlayControl::parentSizeChanged()
{
    hideForZoom();
}

void AutoEditOverlayControl::hideForZoom()
{
    if (! isVisible())
        return;

    if (! hiddenForZoom)
    {
        hiddenForZoom = true;

        // Alpha only - setVisible would end the edit session (see
        // visibilityChanged). The faded control must not swallow clicks
        // meanwhile.
        setInterceptsMouseClicks(false, false);
        juce::Desktop::getInstance().getAnimator()
            .animateComponent(this, getBounds(), 0.0f, fadeMilliseconds, false, 0.0, 0.0);
    }

    // Every further zoom notification pushes the comeback out again, so the
    // control returns once the zoom has settled - not in the middle of it.
    startTimer (comebackDelayMilliseconds);
}

void AutoEditOverlayControl::timerCallback()
{
    stopTimer();

    hiddenForZoom = false;
    setInterceptsMouseClicks(true, true);

    // The clip has a new shape now; snap to the fresh spot and fade back in.
    snapOnNextUpdate = true;
    updatePosition();

    juce::Desktop::getInstance().getAnimator()
        .animateComponent(this, getBounds(), 1.0f, fadeMilliseconds, false, 0.0, 0.0);
}

void AutoEditOverlayControl::changeListenerCallback (juce::ChangeBroadcaster*)
{
    audium::AutoEdit autoEdit(audiumEngine);

    auto config = makeConfig();

    // Only an actual clip selection re-targets the pending edit; deselecting,
    // or selecting something that is no clip, leaves it where it is.
    if (! autoEdit.targetSelectedClip(config))
        return;

    if (config.trackId == trackId && config.playlistItemId == playlistItemId)
        return;

    autoEdit.previewAutoEdit(config);
}

void AutoEditOverlayControl::updatePosition()
{
    auto* parent = getParentComponent();

    if (parent == nullptr)
        return;

    auto area = parent->getLocalBounds();

    // Only the part of the clip inside the viewport is in reach; a clip wider
    // than the arrangement would otherwise centre the control off-screen.
    auto* viewport = findParentComponentOfClass<juce::Viewport>();

    if (viewport != nullptr)
        area = area.getIntersection (parent->getLocalArea (viewport, viewport->getLocalBounds()));

    area.reduce (visibleMargin, 0);

    auto& animator = juce::Desktop::getInstance().getAnimator();

    // The control never squeezes its buttons: full width when it fits,
    // Apply-only when just that fits, and hidden until scrolling or zooming
    // makes room otherwise.
    const auto width = area.getWidth() >= preferredWidth ? preferredWidth
                     : area.getWidth() >= applyOnlyWidth ? applyOnlyWidth
                                                         : 0;

    if (width == 0)
    {
        animator.cancelAnimation(this, false);
        setBounds ({});
        snapOnNextUpdate = true;
        return;
    }

    const auto height = juce::jmin (area.getHeight(), controlHeight);

    const auto ideal = area.withSizeKeepingCentre (width, height);

    // Stillness and drift are judged on screen: scrolling and zooming move
    // the clip - and with it these local coordinates - under the viewport,
    // so only the viewport-relative position says whether anything visibly
    // moved.
    const auto idealOnScreen = viewport != nullptr ? viewport->getLocalArea (parent, ideal)
                                                   : ideal;

    if (snapOnNextUpdate)
    {
        committedTarget = idealOnScreen;
        snapOnNextUpdate = false;
        animator.cancelAnimation(this, false);
        setBounds (ideal);
        return;
    }

    const auto withinDeadZone =
        committedTarget.getPosition().getDistanceFrom (idealOnScreen.getPosition()) <= repositionDistance
        && std::abs (committedTarget.getWidth() - idealOnScreen.getWidth()) <= repositionDistance
        && std::abs (committedTarget.getHeight() - idealOnScreen.getHeight()) <= repositionDistance;

    if (withinDeadZone)
    {
        // Inside the dead zone the control holds its spot on screen while the
        // clip slides underneath, which takes re-pinning it in the clip's
        // moving coordinates on every notification. Gliding towards a parent
        // in motion is exactly what jitters, so this is an instant move - but
        // never while a glide is in flight.
        if (! animator.isAnimating (this) && viewport != nullptr)
        {
            const auto hold = parent->getLocalArea (viewport, committedTarget);

            if (hold != getBounds())
                setBounds (hold);
        }

        return;
    }

    committedTarget = idealOnScreen;

    const auto destination = viewport != nullptr ? parent->getLocalArea (viewport, committedTarget)
                                                 : ideal;

    // Restarting the animation on a new destination re-targets any glide
    // still in flight.
    animator.animateComponent(this, destination, 1.0f, glideMilliseconds, false, 0.0, 0.0);
}
