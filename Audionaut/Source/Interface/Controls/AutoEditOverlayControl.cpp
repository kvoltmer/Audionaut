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
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"

namespace {

constexpr int controlHeight = 28;
constexpr int sliderWidth = 190;
constexpr int buttonWidth = 70;
constexpr int gap = 8;
constexpr int padding = 6;

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

constexpr int preferredWidth = padding + sliderWidth + gap + buttonWidth + padding;

} // namespace

AutoEditOverlayControl::AutoEditOverlayControl(std::shared_ptr<audium::AudiumEngine> audiumEngine_,
                                               std::shared_ptr<RegionSelector> regionSelector_) :
    audiumEngine(audiumEngine_),
    regionSelector(regionSelector_)
{
    setWantsKeyboardFocus (true);

    segmentMeasures = std::make_unique<juce::Slider>("Auto Edit Measures Slider Font 13");
    addAndMakeVisible (segmentMeasures.get());
    AudiumLookAndFeel::configureSlider(segmentMeasures.get());
    segmentMeasures->setColour (juce::Slider::backgroundColourId, juce::Colours::black);
    segmentMeasures->setVelocityModeParameters(1.0, 1, 0.01);
    segmentMeasures->setNormalisableRange(juce::NormalisableRange<double>(0, 64, 1));
    segmentMeasures->onValueChange = [this]() {
        updatePreview();
    };
    segmentMeasures->setTextValueSuffix (" measures");
    segmentMeasures->setValue(defaultMeasures, juce::dontSendNotification);
    segmentMeasures->updateText();

    applyButton = std::make_unique<juce::TextButton> (TRANS ("Apply"));
    addAndMakeVisible (applyButton.get());
    applyButton->onClick = [this]() {
        apply();
    };
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
    config.segmentMeasures = segmentMeasures->getValue();

    return config;
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
}

void AutoEditOverlayControl::resized()
{
    auto r = getLocalBounds().reduced (padding, 3);

    applyButton->setBounds (r.removeFromRight (buttonWidth));
    r.removeFromRight (gap);
    segmentMeasures->setBounds (r);
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
            {
                segmentMeasures->setValue(analysisProvider->getMergePreviewMeasures(),
                                          juce::dontSendNotification);
                segmentMeasures->updateText();
            }

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

    if (area.isEmpty())
    {
        animator.cancelAnimation(this, false);
        setBounds ({});
        snapOnNextUpdate = true;
        return;
    }

    const auto width = juce::jmin (area.getWidth(), preferredWidth);
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
