/*
  ==============================================================================

    MiddlePanelComponent.cpp
    Created: 6 Jun 2023 11:51:48am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include <JuceHeader.h>
#include "MiddlePanelComponent.h"
#include "Interface/Models/WaveFormTableListBoxModel.h"
#include "Interface/Controls/WaveFormTableListBox.h"
#include "Util/EngineAccess.h"
#include "Interface/Handlers/ZoomHandler.h"
#include "Interface/Controls/PlayPositionMarker.h"
#include "Interface/Controls/TransportPositionControl.h"

//==============================================================================
MiddlePanelComponent::MiddlePanelComponent(std::shared_ptr<AudiumEngine> audiumEngine) :
    audiumEngine(audiumEngine)
{
    /// TODO: handle this in a more elegant way
    auto initialWidth = 1000;
    
    zoomHandler.reset(new ZoomHandler(audiumEngine->getAudioResourceContainer(),
                                      audiumEngine->getTransportSourceProvider()));
    waveFormTableListBox.reset(new WaveFormTableListBox("waveform listbox", nullptr));
    regionSelector.reset(new RegionSelector(waveFormTableListBox, zoomHandler, audiumEngine));
    waveFormTableListBoxModel.reset(new WaveFormTableListBoxModel(waveFormTableListBox,
                                                                  audiumEngine->getAudioResourceContainer(),
                                                                  zoomHandler,
                                                                  regionSelector));
    waveFormTableListBox->setModel(waveFormTableListBoxModel.get());
    auto headerComponent = std::unique_ptr<TransportPositionControl> (new TransportPositionControl(waveFormTableListBox, zoomHandler, audiumEngine));
    waveFormTableListBox->setHeaderComponent(std::move(headerComponent));
    waveFormTableListBox->getHeaderComponent()->setSize(initialWidth, 25);
    waveFormTableListBox->setMinimumContentWidth(initialWidth);
    waveFormTableListBox->setOutlineThickness(0);
    
    playPositionMarker.reset(new PlayPositionMarker(zoomHandler, audiumEngine));
    addAndMakeVisible(playPositionMarker.get());

    zoomHandler->setHorizontalScrollBar(&waveFormTableListBox->getHorizontalScrollBar());


    waveFormTableListBox->setMultipleSelectionEnabled(true);
    addAndMakeVisible(waveFormTableListBox.get());

    // selection component
    addAndMakeVisible(regionSelector.get());
    
    
    
    zoomHandler->setWidth(initialWidth);
    
    // make sure the play postition view is on top 
    playPositionMarker->toFront(false);

    updateUI();

}

MiddlePanelComponent::~MiddlePanelComponent()
{
    waveFormTableListBox->setHeaderComponent(nullptr);
    waveFormTableListBox->setModel(nullptr);
    regionSelector = nullptr;
    waveFormTableListBox = nullptr;
    waveFormTableListBoxModel = nullptr;
    zoomHandler = nullptr;
    playPositionMarker = nullptr;
}

void MiddlePanelComponent::paint (juce::Graphics&)
{
}

void MiddlePanelComponent::resized()
{
    waveFormTableListBox->setBounds(getLocalBounds());
    playPositionMarker->setBounds(getLocalBounds());
}

void MiddlePanelComponent::updateUI()
{
    waveFormTableListBox->updateContent();
    regionSelector->updateFromEngine();
}

void MiddlePanelComponent::zoomIn()
{
    auto width = getWidth() * zoomHandler->zoomIn();
    waveFormTableListBox->setMinimumContentWidth(width);
    zoomHandler->setWidth(width);
    regionSelector->updateFromEngine();
    zoomHandler->jumpToPlayPosition();
}

void MiddlePanelComponent::zoomOut()
{
    auto width = getWidth() * zoomHandler->zoomOut();
    waveFormTableListBox->setMinimumContentWidth(width);
    zoomHandler->setWidth(width);
    regionSelector->updateFromEngine();
}
