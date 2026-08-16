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
#include "Interface/Controls/FadeInOutControl.h"
#include "Interface/Controls/AutoEditOverlayControl.h"
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
    fadeInControl = std::make_unique<FadeInOutControl>(FadeInOutControl::FadeIn,
                                                       playListItem,
                                                       regionSelector);
    addAndMakeVisible(fadeInControl.get());
    fadeInControl->setVisible(false);

    
    // FADE OUT
    fadeOutControl = std::make_unique<FadeInOutControl>(FadeInOutControl::FadeOut,
                                                       playListItem,
                                                       regionSelector);
    addAndMakeVisible(fadeOutControl.get());
    fadeOutControl->setVisible(false);

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
    
    // function pointer setup:
    fadeInControl->onValueChange = [this, item] {
        if (item->getDynamics().setFadeIn(fadeInControl->getValue()))
            fadeOutControl->setValue(playListItem->getDynamics().getFadeOut());

        playListItemListBox->updateContent();
        repaintClipRows();
    };
    fadeInControl->onDragStart = [item] { item->onDragStart(); };
    fadeInControl->onDragEnd = [item] { item->onDragEnd(); };
    
    // function pointer setup:
    fadeOutControl->onValueChange = [this, item] {
        if (item->getDynamics().setFadeOut(fadeOutControl->getValue()))
            fadeInControl->setValue(item->getDynamics().getFadeIn());
        playListItemListBox->updateContent();
        repaintClipRows();
    };
    fadeOutControl->onDragStart = [item] { item->onDragStart(); };
    fadeOutControl->onDragEnd = [item] { item->onDragEnd(); };
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

    fadeInControl->setPlayListItem(playListItem);
    fadeOutControl->setPlayListItem(playListItem);

    fadeInControl->setValue(playListItem->getDynamics().getFadeIn());
    fadeOutControl->setValue(playListItem->getDynamics().getFadeOut());

    fadeInControl->setVisible(playListItem->isSelected());
    fadeOutControl->setVisible(playListItem->isSelected());
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
    
    playListItemListBox->updateContent();
}

void PlayListItemComponent::mouseExit (const MouseEvent& e)
{
    if (! isMouseOverOrDragging (true)) {
        fadeInControl->setVisible(false);
        fadeOutControl->setVisible(false);
        playListItemListBox->updateContent();
    }

    
}
