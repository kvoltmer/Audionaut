/*
  ==============================================================================

    RegionPanelComponent.cpp
    Created: 6 Jun 2023 11:50:49am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include <JuceHeader.h>
#include "RegionPanelComponent.h"
#include "Interface/Models/RegionTableListBoxModel.h"
#include "Interface/Controls/RegionTableListBox.h"
#include "Engine/AudiumEngine.h"

//==============================================================================
RegionPanelComponent::RegionPanelComponent(std::shared_ptr<AudiumEngine> audiumEngine) :
    audiumEngine(audiumEngine)
{
    regionTableListBox.reset(new RegionTableListBox());
    regionTableListBoxModel.reset(new RegionTableListBoxModel(regionTableListBox, audiumEngine->getAudioRegionContainer()));
    
    regionTableListBox->setModel(regionTableListBoxModel.get());
    regionTableListBox->setMultipleSelectionEnabled(true);
    regionTableListBox->setColour(juce::TableListBox::backgroundColourId, juce::Colour (0x00000000));
    addAndMakeVisible(regionTableListBox.get());
    
    regionTableListBox->getHeader().addColumn ("name", 1, 250, 80, 800, juce::TableHeaderComponent::notSortable);
    regionTableListBox->getHeader().addColumn ("length", 2, 150, 80, 800, juce::TableHeaderComponent::notSortable);
    
    regionTableListBox->getHeader().setStretchToFitActive (true);

    regionTableListBox->setOutlineThickness (1);
    regionTableListBox->updateContent();
}

RegionPanelComponent::~RegionPanelComponent()
{
    regionTableListBox->setModel(nullptr);
    regionTableListBox = nullptr;
    regionTableListBoxModel = nullptr;
}

void RegionPanelComponent::paint (juce::Graphics&)
{
}

void RegionPanelComponent::resized()
{
    // This method is where you should set the bounds of any child
    // components that your component contains..
    regionTableListBox->setBounds (0, 0, proportionOfWidth (1.0000f), proportionOfHeight (1.0000f));
}

void RegionPanelComponent::updateUI()
{
    /// If your ListBox doesn’t use custom components,
    /// then updateContents() will only update the contents if the number of rows changed, otherwise it does nothing.
    regionTableListBox->updateContent();
    
    auto selectedRow = audiumEngine->getAudioRegionContainer()->getSelectedRegion();
    regionTableListBox->selectRangeOfRows(selectedRow, selectedRow);
    
    /// repaint does the trick
    regionTableListBox->repaint();
}

void RegionPanelComponent::clearSelection()
{
    regionTableListBox->deselectAllRows();
}
