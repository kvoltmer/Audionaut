/*
  ==============================================================================

    WaveFormPanelComponent.cpp
    Created: 6 Jun 2023 11:51:48am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include <JuceHeader.h>
#include "WaveFormPanelComponent.h"
#include "Interface/Models/WaveFormTableListBoxModel.h"
#include "Interface/Controls/WaveFormTableListBox.h"
#include "Util/EngineAccess.h"
#include "Interface/Handlers/ZoomHandler.h"
#include "Interface/Controls/PlayPositionMarker.h"
#include "Interface/Controls/TransportPositionControl.h"

//==============================================================================
WaveFormPanelComponent::WaveFormPanelComponent(std::shared_ptr<AudiumEngine> audiumEngine) :
    audiumEngine(audiumEngine)
{
    /// TODO: handle this in a more elegant way
    auto initialWidth = 1000;
    
    zoomHandler.reset(new ZoomHandler(audiumEngine->getAudioResourceContainer()));
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
    waveFormTableListBox->setColour(juce::TableListBox::backgroundColourId, juce::Colour (0x00000000));
    addAndMakeVisible(waveFormTableListBox.get());

    // selection component
    addAndMakeVisible(regionSelector.get());
    
    
    
    zoomHandler->setWidth(initialWidth);
    
    // make sure the play postition view is on top 
    playPositionMarker->toFront(false);

    updateUI();

}

WaveFormPanelComponent::~WaveFormPanelComponent()
{
    waveFormTableListBox->setHeaderComponent(nullptr);
    waveFormTableListBox->setModel(nullptr);
    regionSelector = nullptr;
    waveFormTableListBox = nullptr;
    waveFormTableListBoxModel = nullptr;
    zoomHandler = nullptr;
    playPositionMarker = nullptr;
}

void WaveFormPanelComponent::paint (juce::Graphics&)
{
}

void WaveFormPanelComponent::resized()
{
    waveFormTableListBox->setBounds(getBounds());
    playPositionMarker->setBounds(getBounds());
}

void WaveFormPanelComponent::updateUI()
{
    waveFormTableListBox->updateContent();
    regionSelector->updateFromEngine();
}

void WaveFormPanelComponent::zoomIn()
{
    auto width = getWidth() * zoomHandler->zoomIn();
    waveFormTableListBox->setMinimumContentWidth(width);
    zoomHandler->setWidth(width);
    regionSelector->updateFromEngine();
}

void WaveFormPanelComponent::zoomOut()
{
    auto width = getWidth() * zoomHandler->zoomOut();
    waveFormTableListBox->setMinimumContentWidth(width);
    zoomHandler->setWidth(width);
    regionSelector->updateFromEngine();
}
