//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <cmath>

#include "ClipOverlayBase.h"
#include "Engine/ActionMessages.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Selection/SelectionManager.h"
#include "Interface/Controls/RegionSelector.h"

namespace {

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

/**
 * The round close chip: a filled circle with a white cross, as an
 * ImageFitted drawable - no rectangular button background. The circle
 * itself defines the drawable's bounds, so no design-box pinning is
 * needed.
 */
std::unique_ptr<juce::Drawable> makeCloseChipFace (juce::Colour circleColour)
{
    auto face = std::make_unique<juce::DrawableComposite>();

    auto circle = std::make_unique<juce::DrawablePath>();
    juce::Path circlePath;
    circlePath.addEllipse (0.0f, 0.0f, 24.0f, 24.0f);
    circle->setPath (circlePath);
    circle->setFill (juce::FillType (circleColour));
    face->addAndMakeVisible (circle.release());

    auto cross = std::make_unique<juce::DrawablePath>();
    juce::Path lines;
    lines.startNewSubPath (8.5f, 8.5f);
    lines.lineTo (15.5f, 15.5f);
    lines.startNewSubPath (15.5f, 8.5f);
    lines.lineTo (8.5f, 15.5f);
    juce::Path crossPath;
    juce::PathStrokeType (2.2f, juce::PathStrokeType::curved,
                          juce::PathStrokeType::rounded).createStrokedPath (crossPath, lines);
    cross->setPath (crossPath);
    cross->setFill (juce::FillType (juce::Colours::white));
    face->addAndMakeVisible (cross.release());

    return face;
}

std::unique_ptr<juce::DrawableButton> makeCloseChip()
{
    auto button = std::make_unique<juce::DrawableButton> (
        juce::String(), juce::DrawableButton::ButtonStyle::ImageFitted);

    const auto normal = makeCloseChipFace (juce::Colours::grey);
    const auto over = makeCloseChipFace (juce::Colours::grey.brighter (0.4f));

    // setImages() deep-copies, so the locals are safe to hand over.
    button->setImages (normal.get(), over.get());

    return button;
}

} // namespace

ClipOverlayBase::ClipOverlayBase(std::shared_ptr<audium::AudiumEngine> audiumEngine_,
                                 std::shared_ptr<RegionSelector> regionSelector_) :
    audiumEngine(audiumEngine_),
    regionSelector(regionSelector_)
{
    setWantsKeyboardFocus (true);

    // The mouse spends its time here over the child widgets, and mouseEnter
    // has to fire for those too (see the header).
    addMouseListener (this, true);

    // The close chip: a small round dismiss button straddling the scrim's
    // top-left corner. Same grey-and-white language as the other buttons,
    // but circular and unlabelled - it is its own symbol.
    closeButton = makeCloseChip();
    closeButton->onClick = [this] { dismissOverlay(); };
    addAndMakeVisible (closeButton.get());
}

ClipOverlayBase::~ClipOverlayBase()
{
    audiumEngine->getAudioTrackContainer()->removeActionListener(this);
    audiumEngine->getAudioTrackContainer()->getSelectionManager()->removeChangeListener(this);
    releaseRegionSelector();
}

void ClipOverlayBase::releaseRegionSelector()
{
    if (regionSelector != nullptr)
        regionSelector->setEnabled(true);
}

void ClipOverlayBase::setTarget(int trackId_, int playlistItemId_)
{
    trackId = trackId_;
    playlistItemId = playlistItemId_;
}

void ClipOverlayBase::pinToDesignBox (juce::Path& path)
{
    path.startNewSubPath (0.0f, 0.0f);
    path.startNewSubPath (24.0f, 24.0f);
}

juce::Path ClipOverlayBase::minusIconPath()
{
    juce::Path path;
    path.addRoundedRectangle (3.0f, 10.75f, 18.0f, 2.5f, 1.25f);
    pinToDesignBox (path);
    return path;
}

juce::Path ClipOverlayBase::plusIconPath()
{
    juce::Path path;
    path.addRoundedRectangle (3.0f, 10.75f, 18.0f, 2.5f, 1.25f);
    path.addRoundedRectangle (10.75f, 3.0f, 2.5f, 18.0f, 1.25f);
    pinToDesignBox (path);
    return path;
}

juce::Path ClipOverlayBase::checkIconPath()
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

juce::Rectangle<int> ClipOverlayBase::getContentArea() const
{
    // symmetric horizontally, so the scrim keeps its size and centring
    return getLocalBounds().withTrimmedLeft (closeButtonOverhang)
                           .withTrimmedRight (closeButtonOverhang)
                           .withTrimmedTop (closeButtonOverhang);
}

void ClipOverlayBase::resized()
{
    // The chip sits at the component's true corner, straddling the scrim's
    // top-left edge by the overhang.
    closeButton->setBounds (0, 0, closeButtonSize, closeButtonSize);
}

std::unique_ptr<juce::DrawableButton> ClipOverlayBase::makeIconButton (const juce::String& label,
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

void ClipOverlayBase::drawLabel (juce::Graphics& g, const juce::Component& component,
                                 const juce::String& text, bool enabled) const
{
    // The label sits below its widget, on the scrim rather than inside the
    // widget. Centred on it, allowed to run wider - the widget spacing
    // keeps neighbouring labels apart.
    const auto labelArea = component.getBounds()
                               .withY (component.getBottom())
                               .withHeight (labelHeight)
                               .expanded (stepGap, 0);

    g.setColour (juce::Colours::white.withMultipliedAlpha (enabled ? 1.0f : 0.4f));
    g.drawFittedText (text, labelArea, juce::Justification::centred, 1);
}

void ClipOverlayBase::paintLabels (juce::Graphics& g)
{
    for (auto* child : getChildren())
        if (auto* button = dynamic_cast<juce::DrawableButton*> (child))
            if (button->isVisible() && button->getName().isNotEmpty())
                drawLabel (g, *button, button->getName(), button->isEnabled());
}

void ClipOverlayBase::paint (juce::Graphics& g)
{
    // A faint scrim, not a window: the waveform underneath stays readable.
    // Inset by the close chip's overhang - the chip hangs over this corner.
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (getContentArea().toFloat(), 5.0f);

    g.setFont (juce::Font (juce::FontOptions (labelFontHeight)));
    paintLabels (g);
}

void ClipOverlayBase::mouseEnter (const juce::MouseEvent&)
{
    // A hover control's mouseExit may have handed the selector back while
    // the session is pending; the takeover is re-asserted the same way
    // visibilityChanged() established it, before a click could start a
    // region selection instead.
    if (regionSelector != nullptr)
    {
        regionSelector->cancelSelection();
        regionSelector->setEnabled(false);
    }
}

bool ClipOverlayBase::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        // Clearing the driving state hides the overlay - visibility follows
        // the data, never a direct hide.
        dismissOverlay();
        return true;
    }

    return false;
}

void ClipOverlayBase::visibilityChanged()
{
    auto audioTrackContainer = audiumEngine->getAudioTrackContainer();
    auto selectionManager = audioTrackContainer->getSelectionManager();

    if (isVisible())
    {
        overlayShown();

        // Only the visible control follows the selection, so a session has
        // exactly one follower.
        selectionManager->addChangeListener(this);

        // The overlay takes over the arrangement: any region selection in
        // progress is dismissed, and none can start while the session is
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
        overlayHidden();

        audioTrackContainer->removeActionListener(this);
        selectionManager->removeChangeListener(this);

        stopTimer();
        juce::Desktop::getInstance().getAnimator().cancelAnimation(this, false);

        // Hiding under the mouse never delivers the mouseExit that would
        // give the selector back.
        releaseRegionSelector();
    }
}

void ClipOverlayBase::actionListenerCallback (const juce::String& message)
{
    // While faded away for a zoom there is nothing to keep in place - the
    // comeback snaps to the fresh spot anyway.
    if (message == audium::scrolledVertically && ! hiddenForZoom)
        updatePosition();

    // An arrangement update means edits (drags, live stretches) are
    // re-laying clips out: follow the resizes in place. If listener
    // ordering let a fade start first, come straight back.
    if (message == audium::updateArrangementAction)
    {
        expectParentResize();

        if (hiddenForZoom)
            restoreAfterZoom();
    }

    overlayActionReceived (message);
}

void ClipOverlayBase::expectParentResize()
{
    // A window rather than a one-shot: a single edit can trigger several
    // resizes, and a live drag streams them continuously.
    followResizesUntil = juce::Time::getMillisecondCounter() + 300;
}

void ClipOverlayBase::parentSizeChanged()
{
    // A resize caused by an edit (a live stretch or drag re-laying the
    // clip out) is followed in place - the zoom fade is for reshapes that
    // arrive without an arrangement broadcast, i.e. zooming.
    if (juce::Time::getMillisecondCounter() < followResizesUntil)
    {
        snapOnNextUpdate = true;
        updatePosition();
        return;
    }

    hideForZoom();
}

void ClipOverlayBase::hideForZoom()
{
    if (! isVisible())
        return;

    if (! hiddenForZoom)
    {
        hiddenForZoom = true;

        // Alpha only - setVisible would end the session (see
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

void ClipOverlayBase::timerCallback()
{
    restoreAfterZoom();
}

void ClipOverlayBase::restoreAfterZoom()
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

void ClipOverlayBase::updatePosition()
{
    auto* parent = getParentComponent();

    if (parent == nullptr)
        return;

    auto area = parent->getLocalBounds();

    // Only the part of the clip inside the viewport is in reach; a clip
    // wider than the arrangement would otherwise centre the control
    // off-screen.
    auto* viewport = findParentComponentOfClass<juce::Viewport>();

    if (viewport != nullptr)
        area = area.getIntersection (parent->getLocalArea (viewport, viewport->getLocalBounds()));

    area.reduce (visibleMargin, 0);

    auto& animator = juce::Desktop::getInstance().getAnimator();

    // The control never squeezes its widgets: full width when it fits, the
    // subclass's minimum when just that fits, and hidden until scrolling or
    // zooming makes room otherwise. The close chip's overhang grows the
    // component beyond the scrim on both sides, keeping the scrim centred.
    const auto preferred = getPreferredWidth() + 2 * closeButtonOverhang;
    const auto minimum = getMinimumWidth() + 2 * closeButtonOverhang;
    const auto width = area.getWidth() >= preferred ? preferred
                     : area.getWidth() >= minimum ? minimum
                                                  : 0;

    if (width == 0)
    {
        animator.cancelAnimation(this, false);
        setBounds ({});
        snapOnNextUpdate = true;
        return;
    }

    const auto height = juce::jmin (area.getHeight(), controlHeight + closeButtonOverhang);

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
        // Inside the dead zone the control holds its spot on screen while
        // the clip slides underneath, which takes re-pinning it in the
        // clip's moving coordinates on every notification. Gliding towards
        // a parent in motion is exactly what jitters, so this is an instant
        // move - but never while a glide is in flight.
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
