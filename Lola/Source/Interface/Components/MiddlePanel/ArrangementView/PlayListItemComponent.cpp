/*
  ==============================================================================

    PlayListItemComponent.cpp
    Created: 28 Sep 2023 12:07:58pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include <JuceHeader.h>
#include "PlayListItemComponent.h"

#include "Engine/PlayList/PlayListItem.h"

#include "Interface/ColourIds.h"
#include "Interface/Controls/PlayListItemDraggerControl.h"
#include "Interface/Controls/LevelMeter.h"
#include "Interface/Controls/FadeInOutControl.h"

PlayListItemComponent::PlayListItemComponent(std::shared_ptr<AudiumEngine> audiumEngine,
                                             std::shared_ptr<AudioTrack> audioTrack,
                                             std::shared_ptr<PlayListContainer> playListContainer,
                                             std::shared_ptr<ZoomHandler> zoomHandler,
                                             std::shared_ptr<RegionSelector> regionSelector) :
    audiumEngine(audiumEngine),
    audioTrack(audioTrack),
    regionSelector(regionSelector)
{
    
    // LISTBOX + MODEL
    playListItemListBox.reset(new audium::ListBox("PlayListItemListBox"));
    addAndMakeVisible(playListItemListBox.get());
    playListItemArrangementModel.reset(new PlayListItemArrangementModel(*playListItemListBox.get(),
                                                                        audioTrack,
                                                                        playListItem,
                                                                        audiumEngine,
                                                                        zoomHandler,
                                                                        regionSelector));
    
    // create dragger as header of ListBox
    auto dragger = std::unique_ptr<PlayListItemDraggerControl> (new PlayListItemDraggerControl(audiumEngine,
                                                                                               playListContainer,
                                                                                               zoomHandler,
                                                                                               audioTrack->getColour(),
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
}

PlayListItemComponent::~PlayListItemComponent()
{
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
        g.setColour (audioTrack->getColour().withAlpha(0.50f));
    }
    g.drawRoundedRectangle (getLocalBounds().toFloat(), 3.0f, 1.0f);
}

void PlayListItemComponent::resized()
{
    playListItemListBox->setBounds(getLocalBounds());
    regionSelector->updateFromEngine();

    updateUI(playListItem);
}

void PlayListItemComponent::changeListenerCallback (ChangeBroadcaster* source)
{
    playListItemListBox->updateContent();
}

DraggerControl* PlayListItemComponent::getDraggerControl() const
{
    return static_cast<DraggerControl*>(playListItemListBox->getHeaderComponent());
}

void PlayListItemComponent::setPlayListItem(std::shared_ptr<PlayListItem> item)
{
    playListItem = item;
    
    // function pointer setup:
    fadeInControl->onValueChange = [this, item] {
        if (item->setFadeIn(fadeInControl->getValue()))
            fadeOutControl->setValue(playListItem->getFadeOut());
        
        playListItemListBox->updateContent();
    };
    fadeInControl->onDragStart = [item] { item->onDragStart(); };
    fadeInControl->onDragEnd = [item] { item->onDragEnd(); };
    
    // function pointer setup:
    fadeOutControl->onValueChange = [this, item] {
        if (item->setFadeOut(fadeOutControl->getValue()))
            fadeInControl->setValue(item->getFadeIn());
        playListItemListBox->updateContent();
    };
    fadeOutControl->onDragStart = [item] { item->onDragStart(); };
    fadeOutControl->onDragEnd = [item] { item->onDragEnd(); };
}

void PlayListItemComponent::updateUI(std::shared_ptr<PlayListItem> item)
{
    setPlayListItem(item);
    if (auto dragger = dynamic_cast<PlayListItemDraggerControl*>(playListItemListBox->getHeaderComponent())) {
        dragger->setPlayListItem(playListItem);
        dragger->setComponentToDrag(getParentComponent());
        dragger->setPositionableObject(playListItem);
    }
    
    playListItemArrangementModel->setPlayListItem(playListItem);
    playListItemArrangementModel->setParentComponent(getParentComponent());
    if (playListItemListBox->getModel() == nullptr)
        playListItemListBox->setModel(playListItemArrangementModel.get());
    playListItemListBox->updateContent();
        

    fadeInControl->setPlayListItem(playListItem);
    fadeOutControl->setPlayListItem(playListItem);

    fadeInControl->setValue(playListItem->getFadeIn());
    fadeOutControl->setValue(playListItem->getFadeOut());
}

void PlayListItemComponent::mouseEnter (const MouseEvent& e)
{
    fadeInControl->setVisible(true);
    fadeOutControl->setVisible(true);
}

void PlayListItemComponent::mouseExit (const MouseEvent& e)
{
    if (! isMouseOverOrDragging (true)) {
        fadeInControl->setVisible(false);
        fadeOutControl->setVisible(false);
    }

    
}
