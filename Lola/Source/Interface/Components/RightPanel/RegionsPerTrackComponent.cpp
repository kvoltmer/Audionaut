/*
  ==============================================================================

    RegionsPerTrackComponent.cpp
    Created: 14 Dec 2024 11:12:58am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include <JuceHeader.h>

#include "RegionsPerTrackComponent.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Region/AudioRegionContainer.h"

RegionsPerTrackComponent::~RegionsPerTrackComponent()
{
    regionListBox->setModel(nullptr);
    regionListBox = nullptr;
    regionContainerModel = nullptr;
}

void RegionsPerTrackComponent::paint (juce::Graphics& g)
{
}

void RegionsPerTrackComponent::resized()
{
    regionListBox->setBounds(getLocalBounds());
}

void RegionsPerTrackComponent::updateSelection()
{
    auto selectedRows = audioTrack->getAudioRegionContainer()->getSelectedRows();
    regionListBox->setSelectedRows(selectedRows, juce::dontSendNotification);
}

void RegionsPerTrackComponent::updateUI()
{
    updateSelection();
    
    regionListBox->updateContent();
    
    regionListBox->getHeader().setColour(juce::TableHeaderComponent::textColourId, audioTrack->getColour());
    regionListBox->getHeader().setColumnName(1, audioTrack->getAudioTrackName());
}
