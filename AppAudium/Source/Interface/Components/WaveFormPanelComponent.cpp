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

//==============================================================================
WaveFormPanelComponent::WaveFormPanelComponent(std::shared_ptr<AudiumEngine> audiumEngine) :
    audiumEngine(audiumEngine)
{
    zoomHandler.reset(new ZoomHandler(audiumEngine->getAudioResourceContainer()));
    waveFormTableListBox.reset(new WaveFormTableListBox("waveform listbox", nullptr));
    regionSelector.reset(new RegionSelector(waveFormTableListBox, zoomHandler, audiumEngine->getAudioRegionContainer()));
    waveFormTableListBoxModel.reset(new WaveFormTableListBoxModel(waveFormTableListBox,
                                                                  audiumEngine->getAudioResourceContainer(),
                                                                  zoomHandler,
                                                                  regionSelector));
    waveFormTableListBox->setModel(waveFormTableListBoxModel.get());



    zoomHandler->setHorizontalScrollBar(&waveFormTableListBox->getHorizontalScrollBar());


    waveFormTableListBox->setMultipleSelectionEnabled(true);
    waveFormTableListBox->setColour(juce::TableListBox::backgroundColourId, juce::Colour (0x00000000));
    addAndMakeVisible(waveFormTableListBox.get());

    // selection component
    addAndMakeVisible(regionSelector.get());
    
    /// TODO: handle this in a more elegant way
    auto initialWidth = 1000;
    waveFormTableListBox->setMinimumContentWidth(initialWidth);
    zoomHandler->setWidth(initialWidth);

    updateUI();

}

WaveFormPanelComponent::~WaveFormPanelComponent()
{
    regionSelector = nullptr;
    waveFormTableListBox->setModel(nullptr);
    waveFormTableListBox = nullptr;
    waveFormTableListBoxModel = nullptr;
    zoomHandler = nullptr;
}

void WaveFormPanelComponent::paint (juce::Graphics&)
{
}

void WaveFormPanelComponent::resized()
{
    if (waveFormTableListBox != nullptr)
    {
        waveFormTableListBox->setBounds(getBounds());
    }
}

void WaveFormPanelComponent::updateUI()
{
    waveFormTableListBox->updateContent();
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
