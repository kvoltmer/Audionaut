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
    
    regionTableListBox->getHeader().addColumn ("name", 1, 150, 80, 400);
    regionTableListBox->getHeader().addColumn ("original file", 2, 350, 80, 800);
    regionTableListBox->getHeader().addColumn ("size", 3, 100, 40, 150);
    //regionTableListBox->getHeader().addColumn ("reload", 4, 100, 100, 100, TableHeaderComponent::notResizableOrSortable);
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
    regionTableListBox->updateContent();
    
    auto selectedRow = audiumEngine->getAudioRegionContainer()->getSelectedRegion();
    regionTableListBox->selectRangeOfRows(selectedRow, selectedRow);
}

void RegionPanelComponent::clearSelection()
{
    regionTableListBox->deselectAllRows();
}
