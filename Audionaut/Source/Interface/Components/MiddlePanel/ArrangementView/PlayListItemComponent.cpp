//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <JuceHeader.h>
#include "PlayListItemComponent.h"

#include "Engine/PlayList/PlayListItem.h"

#include "Interface/ColourIds.h"
#include "Interface/Controls/PlayListItemDraggerControl.h"
#include "Interface/Controls/LevelMeter.h"
#include "Interface/Controls/DraggingHandle.h"
#include "Interface/Controls/AutoEditOverlayControl.h"
#include "Interface/Controls/StretchOverlayControl.h"
#include "Interface/Components/MiddlePanel/ArrangementView/AudioTrackComponent.h"
#include "Interface/Views/ClipFadeOverlay.h"
#include "Interface/Views/AudioClipView.h"
#include "Interface/Views/AutoEditPreviewView.h"
#include "Engine/Analysis/AnalysisProvider.h"
#include "Engine/Group/ResourceGroup.h"
#include "Engine/Region/AudioRegion.h"
#include "Engine/Resource/AudioResource.h"

struct PlayListItemComponent::FadeHandleSpec
{
    std::unique_ptr<DraggingHandle> PlayListItemComponent::* control;
    DraggingHandle::FadeType type;
    bool (*apply) (audium::ClipDynamics&, double value); // true: pushed another fade
    bool movesRamp;   // a ramp end: the bend handles follow every edit
    bool onLane;      // parented to the lane's fade overlay (attachHandlesToLane)
};

std::array<PlayListItemComponent::FadeHandleSpec, 6> PlayListItemComponent::fadeHandleSpecs()
{
    using Dynamics = audium::ClipDynamics;
    return {{
        { &PlayListItemComponent::fadeInControl, DraggingHandle::FadeIn,
          [] (Dynamics& d, double v) { return d.setFadeIn(v); }, true, false },
        { &PlayListItemComponent::fadeOutControl, DraggingHandle::FadeOut,
          [] (Dynamics& d, double v) { return d.setFadeOut(v); }, true, false },
        { &PlayListItemComponent::fadeInStartControl, DraggingHandle::FadeInStart,
          [] (Dynamics& d, double v) { return d.setFadeInStart(v); }, true, true },
        { &PlayListItemComponent::fadeOutEndControl, DraggingHandle::FadeOutEnd,
          [] (Dynamics& d, double v) { return d.setFadeOutEnd(v); }, true, true },
        // the bend handles set the curve exponents - no pushes
        { &PlayListItemComponent::curveInControl, DraggingHandle::CurveIn,
          [] (Dynamics& d, double v) { d.setFadeInCurve(v); return false; }, false, true },
        { &PlayListItemComponent::curveOutControl, DraggingHandle::CurveOut,
          [] (Dynamics& d, double v) { d.setFadeOutCurve(v); return false; }, false, true },
    }};
}

PlayListItemComponent::PlayListItemComponent(std::shared_ptr<audium::AudiumEngine> audiumEngine,
                                             std::shared_ptr<audium::AudioTrack> audioTrack,
                                             std::shared_ptr<audium::PlayListContainer> playListContainer,
                                             std::shared_ptr<ZoomHandler> zoomHandler,
                                             std::shared_ptr<RegionSelector> regionSelector) :
    audiumEngine(audiumEngine),
    audioTrack(audioTrack),
    regionSelector(regionSelector)
{
    
    // LISTBOX + MODEL
    playListItemListBox.reset(new audium::ListBox("PlayListItemListBox"));
    addAndMakeVisible(playListItemListBox.get());
    playListItemListBoxModel.reset(new PlayListItemListBoxModel(*playListItemListBox.get(),
                                                                        audioTrack,
                                                                        playListItem,
                                                                        audiumEngine,
                                                                        zoomHandler,
                                                                        regionSelector));
    
    // create dragger as header of ListBox
    auto dragger = std::unique_ptr<PlayListItemDraggerControl> (new PlayListItemDraggerControl(audiumEngine,
                                                                                               playListContainer,
                                                                                               zoomHandler,
                                                                                               audioTrack->getViewState().getColour(),
                                                                                               regionSelector));
    dragger->addChangeListener(this);
    playListItemListBox->setHeaderComponent(std::move(dragger));
    playListItemListBox->getHeaderComponent()->setSize(getWidth(), DraggerControl::draggerHeight);
    playListItemListBox->setOutlineThickness(0);
    playListItemListBox->setColour(audium::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    
    // NOTE: workaround since list box is eating all mouse events
    playListItemListBox->addMouseListener (this, true);
    
    
    // FADE HANDLES (ramp ends on the top strip, silence boundaries on the
    // bottom edge, bend handles on each ramp's midpoint)
    for (const auto& spec : fadeHandleSpecs())
    {
        auto& handle = this->*spec.control;
        handle = std::make_unique<DraggingHandle>(spec.type, playListItem, regionSelector);
        addAndMakeVisible(handle.get());
        handle->setVisible(false);
    }

    // OVERLAY CONTAINER (topmost; transparent itself, children may take clicks)
    overlayContainer = std::make_unique<juce::Component>("PlayListItemOverlay");
    overlayContainer->setInterceptsMouseClicks(false, true);
    addAndMakeVisible(overlayContainer.get());

    // AUTO EDIT PREVIEW (pending auto edit's cuts, spanning the whole clip)
    autoEditPreviewView = std::make_unique<AutoEditPreviewView>();
    autoEditPreviewView->setZoomHandler(zoomHandler);
    autoEditPreviewView->setInterceptsMouseClicks(false, false);
    overlayContainer->addAndMakeVisible(autoEditPreviewView.get());

    // AUTO EDIT CONTROL (measures + Apply, shown while this clip is previewed)
    autoEditOverlayControl = std::make_unique<AutoEditOverlayControl>(audiumEngine, regionSelector);
    overlayContainer->addChildComponent(autoEditOverlayControl.get());

    // STRETCH CONTROL (speed slider + semitone steps, shown while this clip
    // is the overlay target)
    stretchOverlayControl = std::make_unique<StretchOverlayControl>(audiumEngine, regionSelector);
    overlayContainer->addChildComponent(stretchOverlayControl.get());

    // Follow the preview published/cleared by the Auto Edit window.
    if (auto analysisProvider = audiumEngine->getAudioTrackContainer()->getAnalysisProvider())
        analysisProvider->addChangeListener(this);

    // Follow the stretch overlay target set/cleared by the Stretch command.
    audiumEngine->getAudioTrackContainer()->getClipOverlayTarget()->addChangeListener(this);
}

PlayListItemComponent::~PlayListItemComponent()
{
    if (observedClipRect != nullptr)
        observedClipRect->removeComponentListener(this);

    if (auto analysisProvider = audiumEngine->getAudioTrackContainer()->getAnalysisProvider())
        analysisProvider->removeChangeListener(this);

    audiumEngine->getAudioTrackContainer()->getClipOverlayTarget()->removeChangeListener(this);

    playListItemListBox->setModel(nullptr);
}

void PlayListItemComponent::paint (juce::Graphics& g)
{
    if (playListItem->isSelected())
    {
        g.setColour (juce::Colours::white.withAlpha(0.9f));
    }
    else
    {
        g.setColour (audioTrack->getViewState().getColour().withAlpha(0.50f));
    }
    g.drawRoundedRectangle (getLocalBounds().toFloat(), 3.0f, 1.0f);
}

void PlayListItemComponent::resized()
{
    playListItemListBox->setBounds(getLocalBounds());

    // The overlay starts below the dragger header, covering the channel rows.
    overlayContainer->setBounds(getLocalBounds().withTrimmedTop(DraggerControl::draggerHeight));
    overlayContainer->toFront(false);
    autoEditPreviewView->setBounds(overlayContainer->getLocalBounds());
}

void PlayListItemComponent::changeListenerCallback (ChangeBroadcaster* source)
{
    auto analysisProvider = audiumEngine->getAudioTrackContainer()->getAnalysisProvider();

    if (analysisProvider != nullptr && source == analysisProvider.get())
    {
        refreshAutoEditPreview();
        return;
    }

    if (source == audiumEngine->getAudioTrackContainer()->getClipOverlayTarget().get())
    {
        refreshStretchOverlay();
        return;
    }

    // The dragger control broadcast a change.
    playListItemListBox->updateContent();
}

DraggerControl* PlayListItemComponent::getDraggerControl() const
{
    return static_cast<DraggerControl*>(playListItemListBox->getHeaderComponent());
}

void PlayListItemComponent::setPlayListItem(std::shared_ptr<audium::PlayListItem> item)
{
    playListItem = item;
    
    
    for (const auto& spec : fadeHandleSpecs())
    {
        auto* handle = (this->*spec.control).get();
        handle->setPlayListItem(item);

        // mouseExit must reach this clip component - the bottom handles are
        // parented to the lane overlay, not to us
        handle->setExitTarget(this);

        // a pushed fade value can cascade through the other three, so re-sync
        // every handle whenever the setter reports one; any ramp edit moves
        // the bend handles
        handle->onValueChange = [this, item, handle, spec] {
            if (spec.apply(item->getDynamics(), handle->getValue()))
                syncFadeControls();
            if (spec.movesRamp)
                syncCurveControls();

            playListItemListBox->updateContent();
            repaintClipRows();
            repaintFadeOverlay();
        };
        handle->onDragStart = [item] { item->onDragStart(); };
        handle->onDragEnd = [item] { item->onDragEnd(); };
    }
}

void PlayListItemComponent::syncFadeControls()
{
    if (playListItem == nullptr)
        return;

    // the bottom-edge and bend handles are anchored to the first channel row
    if (auto firstChannel = audioTrack->getChannel(0)) {
        auto bottomY = DraggerControl::draggerHeight + firstChannel->getChannelHeight();
        fadeInStartControl->setBottomAnchorY(bottomY);
        fadeOutEndControl->setBottomAnchorY(bottomY);
        curveInControl->setBottomAnchorY(bottomY);
        curveOutControl->setBottomAnchorY(bottomY);
    }

    auto& dynamics = playListItem->getDynamics();

    // once the lane-parented handles live in the overlay, their value math
    // maps against the clip rect (the parent ItemComponent) in lane coords
    auto clipRect = getParentComponent();
    if (clipRect != nullptr && fadeInStartControl->getParentComponent() != this) {
        auto laneRange = clipRect->getBounds().getHorizontalRange();
        fadeInStartControl->setClipRange(laneRange);
        fadeOutEndControl->setClipRange(laneRange);
    }

    fadeInControl->setValue(dynamics.getFadeIn());
    fadeOutControl->setValue(dynamics.getFadeOut());
    fadeInStartControl->setValue(dynamics.getFadeInStart());
    fadeOutEndControl->setValue(dynamics.getFadeOutEnd());

    syncCurveControls();
}

void PlayListItemComponent::syncCurveControls()
{
    if (playListItem == nullptr)
        return;

    auto clipRect = getParentComponent();
    if (clipRect == nullptr || curveInControl->getParentComponent() == this)
        return;

    auto& dynamics = playListItem->getDynamics();
    auto laneRange = clipRect->getBounds().getHorizontalRange();

    // the bend handles sit on their ramp's midpoint (clipRange = the RAMP's
    // x-range in lane coords)
    auto clipX = laneRange.getStart();
    auto clipW = laneRange.getLength();
    auto inRamp = juce::Range<int>(clipX + static_cast<int>(dynamics.getFadeInStart() * clipW),
                                   clipX + static_cast<int>(dynamics.getFadeIn() * clipW));
    auto outRamp = juce::Range<int>(clipX + clipW - static_cast<int>(dynamics.getFadeOut() * clipW),
                                    clipX + clipW - static_cast<int>(dynamics.getFadeOutEnd() * clipW));
    curveInControl->setClipRange(inRamp);
    curveOutControl->setClipRange(outRamp);

    curveInControl->setValue(dynamics.getFadeInCurve());
    curveOutControl->setValue(dynamics.getFadeOutCurve());

    // a bend handle needs a ramp wide enough to grab; follow live shrinking
    // and growing while the other handles are shown
    fadeInRampWide = inRamp.getLength() > 16;
    fadeOutRampWide = outRamp.getLength() > 16;

    if (! fadeInRampWide)
        curveInControl->setVisible(false);
    else if (fadeInControl->isVisible())
        curveInControl->setVisible(true);

    if (! fadeOutRampWide)
        curveOutControl->setVisible(false);
    else if (fadeOutControl->isVisible())
        curveOutControl->setVisible(true);
}

void PlayListItemComponent::attachHandlesToLane()
{
    auto clipRect = getParentComponent(); // the lane's ItemComponent
    if (clipRect == nullptr)
        return;

    auto lane = dynamic_cast<AudioTrackComponent*>(clipRect->getParentComponent());
    if (lane == nullptr || lane->getFadeOverlay() == nullptr)
        return;

    auto overlay = lane->getFadeOverlay();
    for (const auto& spec : fadeHandleSpecs())
        if (auto* handle = (this->*spec.control).get(); spec.onLane && handle->getParentComponent() != overlay)
            overlay->addChildComponent(handle);

    if (observedClipRect != clipRect) {
        if (observedClipRect != nullptr)
            observedClipRect->removeComponentListener(this);
        clipRect->addComponentListener(this);
        observedClipRect = clipRect;
    }
}

void PlayListItemComponent::repaintFadeOverlay()
{
    if (auto clipRect = getParentComponent())
        if (auto lane = dynamic_cast<AudioTrackComponent*>(clipRect->getParentComponent()))
            if (auto overlay = lane->getFadeOverlay())
                overlay->repaint();
}

void PlayListItemComponent::updateHandleVisibility()
{
    const auto shown = playListItem != nullptr
                       && playListItem->isSelected()
                       && ! audioTrack->getViewState().getMinimized();

    fadeInControl->setVisible(shown);
    fadeOutControl->setVisible(shown);
    fadeInStartControl->setVisible(shown);
    fadeOutEndControl->setVisible(shown);
    curveInControl->setVisible(shown && fadeInRampWide);
    curveOutControl->setVisible(shown && fadeOutRampWide);
}

void PlayListItemComponent::componentMovedOrResized (juce::Component& component, bool wasMoved, bool wasResized)
{
    if (&component == observedClipRect) {
        // minimizing/restoring the track arrives here as a lane resize
        updateHandleVisibility();
        syncFadeControls();
        repaintFadeOverlay();
    }
}

void PlayListItemComponent::componentVisibilityChanged (juce::Component& component)
{
    // mirror the lane's culling: an offscreen clip must not leave its
    // lane-parented handles floating
    if (&component == observedClipRect && ! component.isVisible())
        for (const auto& spec : fadeHandleSpecs())
            if (spec.onLane)
                (this->*spec.control)->setVisible(false);
}

void PlayListItemComponent::updateUI(std::shared_ptr<audium::PlayListItem> item)
{
    setPlayListItem(item);
    if (auto dragger = dynamic_cast<PlayListItemDraggerControl*>(playListItemListBox->getHeaderComponent())) {
        dragger->setPlayListItem(playListItem);
        dragger->setComponentToDrag(getParentComponent());
        dragger->setPositionableObject(playListItem);
    }
    
    playListItemListBoxModel->setPlayListItem(playListItem);
    playListItemListBoxModel->setParentComponent(getParentComponent());
    playListItemListBox->setModel(playListItemListBoxModel.get());
    // Not calling playListItemListBox->updateContent() here: it recycles row
    // components across rows and previously caused clips to show the wrong
    // waveform (see "bugfix: wrong waveform in clip"). refreshAnalysisDisplay()
    // updates the analysis overlay on the existing row components instead.
    refreshAnalysisDisplay();
    refreshTrackColour();

    attachHandlesToLane();
    syncFadeControls();

    updateHandleVisibility();

    // the fade curves draw only while selected - selection changes arrive
    // through this update, so refresh the rows and the lane overlay
    repaintClipRows();
    repaintFadeOverlay();
}

void PlayListItemComponent::repaintClipRows()
{
    // The fade overlay (FadeInOutView) is painted by the clip's row components,
    // which nothing repaints on a fade drag since the listbox force-repaints
    // were removed - repaint them explicitly.
    for (int row = 0; row < playListItemListBoxModel->getNumRows(); ++row)
        if (auto* rowComponent = playListItemListBox->getComponentForRowNumber(row))
            rowComponent->repaint();
}

void PlayListItemComponent::refreshAnalysisDisplay()
{
    for (int row = 0; row < playListItemListBoxModel->getNumRows(); ++row)
        if (auto* clipView = dynamic_cast<AudioClipView*>(playListItemListBox->getComponentForRowNumber(row)))
            clipView->refreshSegments();

    refreshAutoEditPreview();
    refreshStretchOverlay();
}

void PlayListItemComponent::refreshTrackColour()
{
    const auto colour = audioTrack->getViewState().getColour();
    for (int row = 0; row < playListItemListBoxModel->getNumRows(); ++row)
        if (auto* clipView = dynamic_cast<AudioClipView*>(playListItemListBox->getComponentForRowNumber(row)))
            clipView->setTrackColour (colour);
}

void PlayListItemComponent::refreshStretchOverlay()
{
    if (playListItem == nullptr || audioTrack == nullptr)
        return;

    auto target = audiumEngine->getAudioTrackContainer()->getClipOverlayTarget();

    stretchOverlayControl->setTarget(audioTrack->getId(), playListItem->getId());

    // The visibility transition is the session boundary: showing syncs the
    // slider and arms the session, hiding commits it as one undo step.
    stretchOverlayControl->setVisible(target->matches(audioTrack->getId(), playListItem->getId()));
}

void PlayListItemComponent::refreshAutoEditPreview()
{
    if (playListItem == nullptr)
        return;

    auto region = playListItem->getRegion();
    if (region == nullptr)
        return;

    auto analysisProvider = audiumEngine->getAudioTrackContainer()->getAnalysisProvider();
    if (analysisProvider == nullptr)
        return;

    // The preview is keyed by the analysed file - the region's resource - and
    // by the target clip's identity, so it follows the selection.
    std::vector<float> preview;
    if (auto resourceGroup = region->getResourceGroup())
    {
        auto resources = resourceGroup->getAudioResources();

        // A clip that is still recording has no file yet, so it cannot have
        // been analysed either.
        if (! resources.empty() && resources[0] != nullptr && ! resources[0]->isRecording())
            preview = analysisProvider->getMergePreview(resources[0]->getLocalFile(),
                                                        audioTrack->getId(),
                                                        playListItem->getId());
    }

    const auto previewActive = ! preview.empty();

    autoEditPreviewView->setPreviewSegments(std::move(preview),
                                            region->getRegionData(audium::seconds).getStart(),
                                            playListItem->getSpeedRatio());

    // The previewed clip also carries the edit's control.
    autoEditOverlayControl->setTarget(audioTrack->getId(), playListItem->getId());
    autoEditOverlayControl->setVisible(previewActive);
}

void PlayListItemComponent::mouseEnter (const MouseEvent& e)
{
    updateHandleVisibility();

    playListItemListBox->updateContent();
}

void PlayListItemComponent::mouseExit (const MouseEvent& e)
{
    // the lane-parented bottom handles are not our descendants, so
    // isMouseOverOrDragging(true) cannot see them - check them explicitly or
    // hovering a handle hanging outside the clip would hide it mid-approach
    if (! isMouseOverOrDragging (true)
        && ! fadeInStartControl->isMouseOverOrDragging()
        && ! fadeOutEndControl->isMouseOverOrDragging()
        && ! curveInControl->isMouseOverOrDragging()
        && ! curveOutControl->isMouseOverOrDragging()) {
        fadeInControl->setVisible(false);
        fadeOutControl->setVisible(false);

        // a lane handle sitting outside the clip stays visible while the
        // clip is selected - hidden, it could never be reached again (the
        // mouse leaves the clip before it gets there)
        auto selected = playListItem->isSelected();
        auto& dynamics = playListItem->getDynamics();
        fadeInStartControl->setVisible(selected && dynamics.getFadeInStart() < 0.0);
        fadeOutEndControl->setVisible(selected && dynamics.getFadeOutEnd() < 0.0);
        curveInControl->setVisible(selected && fadeInRampWide && dynamics.getFadeInStart() < 0.0);
        curveOutControl->setVisible(selected && fadeOutRampWide && dynamics.getFadeOutEnd() < 0.0);
        playListItemListBox->updateContent();
    }

    
}
