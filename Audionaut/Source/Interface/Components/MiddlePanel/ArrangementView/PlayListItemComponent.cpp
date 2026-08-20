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
#include "Interface/Components/MiddlePanel/ArrangementView/AudioTrackComponent.h"
#include "Interface/Views/ClipFadeOverlay.h"
#include "Interface/Views/AudioClipView.h"
#include "Interface/Views/AutoEditPreviewView.h"
#include "Engine/Analysis/AnalysisProvider.h"
#include "Engine/Group/ResourceGroup.h"
#include "Engine/Region/AudioRegion.h"
#include "Engine/Resource/AudioResource.h"

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
    
    
    // FADE IN
    fadeInControl = std::make_unique<DraggingHandle>(DraggingHandle::FadeIn,
                                                       playListItem,
                                                       regionSelector);
    addAndMakeVisible(fadeInControl.get());
    fadeInControl->setVisible(false);

    
    // FADE OUT
    fadeOutControl = std::make_unique<DraggingHandle>(DraggingHandle::FadeOut,
                                                       playListItem,
                                                       regionSelector);
    addAndMakeVisible(fadeOutControl.get());
    fadeOutControl->setVisible(false);

    // FADE IN START (silence boundary, bottom edge)
    fadeInStartControl = std::make_unique<DraggingHandle>(DraggingHandle::FadeInStart,
                                                            playListItem,
                                                            regionSelector);
    addAndMakeVisible(fadeInStartControl.get());
    fadeInStartControl->setVisible(false);

    // FADE OUT END (silence boundary, bottom edge)
    fadeOutEndControl = std::make_unique<DraggingHandle>(DraggingHandle::FadeOutEnd,
                                                           playListItem,
                                                           regionSelector);
    addAndMakeVisible(fadeOutEndControl.get());
    fadeOutEndControl->setVisible(false);

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

    // Follow the preview published/cleared by the Auto Edit window.
    if (auto analysisProvider = audiumEngine->getAudioTrackContainer()->getAnalysisProvider())
        analysisProvider->addChangeListener(this);
}

PlayListItemComponent::~PlayListItemComponent()
{
    if (observedClipRect != nullptr)
        observedClipRect->removeComponentListener(this);

    if (auto analysisProvider = audiumEngine->getAudioTrackContainer()->getAnalysisProvider())
        analysisProvider->removeChangeListener(this);

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
    
    
    fadeInControl->setPlayListItem(item);
    fadeOutControl->setPlayListItem(item);
    fadeInStartControl->setPlayListItem(item);
    fadeOutEndControl->setPlayListItem(item);

    // mouseExit must reach this clip component - the bottom handles are
    // parented to the lane overlay, not to us
    fadeInControl->setExitTarget(this);
    fadeOutControl->setExitTarget(this);
    fadeInStartControl->setExitTarget(this);
    fadeOutEndControl->setExitTarget(this);

    // function pointer setup; a pushed fade value can cascade through the
    // other three, so re-sync every handle whenever the setter reports one
    fadeInControl->onValueChange = [this, item] {
        if (item->getDynamics().setFadeIn(fadeInControl->getValue()))
            syncFadeControls();

        playListItemListBox->updateContent();
        repaintClipRows();
        repaintFadeOverlay();
    };
    fadeInControl->onDragStart = [item] { item->onDragStart(); };
    fadeInControl->onDragEnd = [item] { item->onDragEnd(); };

    // function pointer setup:
    fadeOutControl->onValueChange = [this, item] {
        if (item->getDynamics().setFadeOut(fadeOutControl->getValue()))
            syncFadeControls();
        playListItemListBox->updateContent();
        repaintClipRows();
        repaintFadeOverlay();
    };
    fadeOutControl->onDragStart = [item] { item->onDragStart(); };
    fadeOutControl->onDragEnd = [item] { item->onDragEnd(); };

    // function pointer setup:
    fadeInStartControl->onValueChange = [this, item] {
        if (item->getDynamics().setFadeInStart(fadeInStartControl->getValue()))
            syncFadeControls();
        playListItemListBox->updateContent();
        repaintClipRows();
        repaintFadeOverlay();
    };
    fadeInStartControl->onDragStart = [item] { item->onDragStart(); };
    fadeInStartControl->onDragEnd = [item] { item->onDragEnd(); };

    // function pointer setup:
    fadeOutEndControl->onValueChange = [this, item] {
        if (item->getDynamics().setFadeOutEnd(fadeOutEndControl->getValue()))
            syncFadeControls();
        playListItemListBox->updateContent();
        repaintClipRows();
        repaintFadeOverlay();
    };
    fadeOutEndControl->onDragStart = [item] { item->onDragStart(); };
    fadeOutEndControl->onDragEnd = [item] { item->onDragEnd(); };
}

void PlayListItemComponent::syncFadeControls()
{
    if (playListItem == nullptr)
        return;

    // the bottom-edge handles sit on the bottom of the first channel row
    if (auto firstChannel = audioTrack->getChannel(0)) {
        auto bottomY = DraggerControl::draggerHeight + firstChannel->getChannelHeight();
        fadeInStartControl->setBottomAnchorY(bottomY);
        fadeOutEndControl->setBottomAnchorY(bottomY);
    }

    // once the bottom handles live in the lane overlay, their value math
    // maps against the clip rect (the parent ItemComponent) in lane coords
    auto clipRect = getParentComponent();
    if (clipRect != nullptr && fadeInStartControl->getParentComponent() != this) {
        auto laneRange = clipRect->getBounds().getHorizontalRange();
        fadeInStartControl->setClipRange(laneRange);
        fadeOutEndControl->setClipRange(laneRange);
    }

    auto& dynamics = playListItem->getDynamics();
    fadeInControl->setValue(dynamics.getFadeIn());
    fadeOutControl->setValue(dynamics.getFadeOut());
    fadeInStartControl->setValue(dynamics.getFadeInStart());
    fadeOutEndControl->setValue(dynamics.getFadeOutEnd());
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
    if (fadeInStartControl->getParentComponent() != overlay)
        overlay->addChildComponent(fadeInStartControl.get());
    if (fadeOutEndControl->getParentComponent() != overlay)
        overlay->addChildComponent(fadeOutEndControl.get());

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

void PlayListItemComponent::componentMovedOrResized (juce::Component& component, bool wasMoved, bool wasResized)
{
    if (&component == observedClipRect) {
        syncFadeControls();
        repaintFadeOverlay();
    }
}

void PlayListItemComponent::componentVisibilityChanged (juce::Component& component)
{
    // mirror the lane's culling: an offscreen clip must not leave its
    // lane-parented handles floating
    if (&component == observedClipRect && ! component.isVisible()) {
        fadeInStartControl->setVisible(false);
        fadeOutEndControl->setVisible(false);
    }
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

    attachHandlesToLane();
    syncFadeControls();

    fadeInControl->setVisible(playListItem->isSelected());
    fadeOutControl->setVisible(playListItem->isSelected());
    fadeInStartControl->setVisible(playListItem->isSelected());
    fadeOutEndControl->setVisible(playListItem->isSelected());
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
        if (! resources.empty())
            preview = analysisProvider->getMergePreview(juce::File(resources[0]->getFullPathName()),
                                                        audioTrack->getId(),
                                                        playListItem->getId());
    }

    const auto previewActive = ! preview.empty();

    autoEditPreviewView->setPreviewSegments(std::move(preview),
                                            region->getRegionData(audium::seconds).getStart());

    // The previewed clip also carries the edit's control.
    autoEditOverlayControl->setTarget(audioTrack->getId(), playListItem->getId());
    autoEditOverlayControl->setVisible(previewActive);
}

void PlayListItemComponent::mouseEnter (const MouseEvent& e)
{
    fadeInControl->setVisible(playListItem->isSelected());
    fadeOutControl->setVisible(playListItem->isSelected());
    fadeInStartControl->setVisible(playListItem->isSelected());
    fadeOutEndControl->setVisible(playListItem->isSelected());

    playListItemListBox->updateContent();
}

void PlayListItemComponent::mouseExit (const MouseEvent& e)
{
    // the lane-parented bottom handles are not our descendants, so
    // isMouseOverOrDragging(true) cannot see them - check them explicitly or
    // hovering a handle hanging outside the clip would hide it mid-approach
    if (! isMouseOverOrDragging (true)
        && ! fadeInStartControl->isMouseOverOrDragging()
        && ! fadeOutEndControl->isMouseOverOrDragging()) {
        fadeInControl->setVisible(false);
        fadeOutControl->setVisible(false);

        // a bottom handle sitting outside the clip stays visible while the
        // clip is selected - hidden, it could never be reached again (the
        // mouse leaves the clip before it gets there)
        auto selected = playListItem->isSelected();
        auto& dynamics = playListItem->getDynamics();
        fadeInStartControl->setVisible(selected && dynamics.getFadeInStart() < 0.0);
        fadeOutEndControl->setVisible(selected && dynamics.getFadeOutEnd() < 0.0);
        playListItemListBox->updateContent();
    }

    
}
