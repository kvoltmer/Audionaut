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
#include "Interface/Components/MiddlePanel/ChannelView/ChannelComponent.h"
#include "Interface/Controls/LevelMeter.h"
#include "Interface/Controls/FadeInOutControl.h"

PlayListItemComponent::PlayListItemComponent(std::shared_ptr<AudiumEngine> audiumEngine,
                                             std::shared_ptr<AudioTrack> audioTrack,
                                             std::shared_ptr<PlayListContainer> playListContainer,
                                             std::shared_ptr<PlayListItem> playListItem,
                                             std::shared_ptr<ZoomHandler> zoomHandler,
                                             std::shared_ptr<RegionSelector> regionSelector) :
    audiumEngine(audiumEngine),
    audioTrack(audioTrack),
    playListItem(playListItem),
    regionSelector(regionSelector)
{
    
    playListItemListBox.reset(new audium::ListBox());
    addAndMakeVisible(playListItemListBox.get());
    playListItemArrangementModel.reset(new PlayListItemArrangementModel(*playListItemListBox.get(),
                                                                        audioTrack,
                                                                        playListItem,
                                                                        audiumEngine,
                                                                        zoomHandler,
                                                                        regionSelector));

    playListItemListBox->setModel(playListItemArrangementModel.get());
    
    // create dragger as header of ListBox
    auto dragger = std::unique_ptr<PlayListItemDraggerControl>(new PlayListItemDraggerControl(  this,
                                                                                                audiumEngine,
                                                                                                playListContainer,
                                                                                                playListItem,
                                                                                                zoomHandler,
                                                                                                audioTrack->getColour(),
                                                                                                regionSelector));
    dragger->addChangeListener(this);
    playListItemListBox->setHeaderComponent(std::move(dragger));

    
    playListItemListBox->getHeaderComponent()->setSize(getWidth(), DraggerControl::draggerHeight);
    playListItemListBox->setOutlineThickness(0);
    // transparent backgroud:
    playListItemListBox->setColour(audium::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    
    // note: list box is eating all mouse events
    playListItemListBox->addMouseListener (this, true);
    
    // volume slider
    volumeSlider = std::make_unique<SliderControl>(juce::String(), regionSelector);
    addAndMakeVisible(volumeSlider.get());
    ChannelComponent::configureVolumeSlider(volumeSlider.get(), 36.0);
    
    volumeSlider->onValueChange = [this, playListItem] {
        playListItem->setGain(Decibels::decibelsToGain(volumeSlider->getValue()), true);
        playListItemListBox->updateContent();
    };
    volumeSlider->onDragStart = [playListItem] {
        playListItem->onDragStart();
    };
    
    volumeSlider->onDragEnd = [playListItem] {
        playListItem->onDragEnd();
    };
    
    fadeInControl = std::make_unique<FadeInOutControl>(FadeInOutControl::FadeIn,
                                                       playListItem,
                                                       regionSelector);
    addAndMakeVisible(fadeInControl.get());
    fadeInControl->setVisible(false);
    fadeInControl->onValueChange = [this, playListItem] {
        std::cout << "fade in: " << fadeInControl->getValue() << std::endl;
    };
    fadeInControl->onDragStart = [playListItem] { playListItem->onDragStart(); };
    fadeInControl->onDragEnd = [playListItem] { playListItem->onDragEnd(); };
    
    
    fadeOutControl = std::make_unique<FadeInOutControl>(FadeInOutControl::FadeOut,
                                                       playListItem,
                                                       regionSelector);
    addAndMakeVisible(fadeOutControl.get());
    fadeOutControl->setVisible(false);
    fadeOutControl->onValueChange = [this, playListItem] {
        std::cout << "fade out: " << fadeOutControl->getValue() << std::endl;
    };
    fadeOutControl->onDragStart = [playListItem] { playListItem->onDragStart(); };
    fadeOutControl->onDragEnd = [playListItem] { playListItem->onDragEnd(); };
    
    updateUI();
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
    
    auto sliderWidth = 67;
    auto sliderHeight = 15;
    auto space = 5;
    
    if (getWidth() > sliderWidth + (space * 2)) {
        volumeSlider->setVisible(true);
        volumeSlider->setBounds (space,
                                 getHeight() - sliderHeight - space,
                                 sliderWidth,
                                 sliderHeight);
    }
    else {
        volumeSlider->setVisible(false);
    }
    
    auto draggerHeight = playListItemListBox->getHeaderComponent()->getHeight();
    fadeInControl->setBounds(0, draggerHeight, 15, 15);
    fadeOutControl->setBounds(getWidth() - 15, draggerHeight, 15, 15);
    
}

void PlayListItemComponent::changeListenerCallback (ChangeBroadcaster* source)
{
    playListItemListBox->updateContent();
}

DraggerControl* PlayListItemComponent::getDraggerControl() const
{
    return static_cast<DraggerControl*>(playListItemListBox->getHeaderComponent());
}

void PlayListItemComponent::updateUI()
{
    volumeSlider->setValue(LevelMeter::gainToDecebel(playListItem->getGain()), dontSendNotification);
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
