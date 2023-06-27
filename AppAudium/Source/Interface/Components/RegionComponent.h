/*
  ==============================================================================

    RegionComponent.h
    Created: 27 Jun 2023 2:33:20pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "Engine/AudiumEngine.h"
#include "Interface/Models/RegionTableListBoxModel.h"
#include "Interface/Controls/RegionTableListBox.h"

//==============================================================================
/*
*/

class RegionComponent  : public juce::Component
{
public:
    RegionComponent(std::shared_ptr<AudiumEngine> audiumEngine) :
        audiumEngine(audiumEngine)
    {
        regionTableListBox.reset(new RegionTableListBox());
        regionTableListBoxModel.reset(new RegionTableListBoxModel(regionTableListBox, audiumEngine->getAudioRegionContainer()));

        regionTableListBox->setModel(regionTableListBoxModel.get());
        regionTableListBox->setMultipleSelectionEnabled(true);
        addAndMakeVisible(regionTableListBox.get());
        
        regionTableListBox->getHeader().addColumn ("Regions", 1, 250, 80, 800, juce::TableHeaderComponent::notSortable);
        regionTableListBox->getHeader().addColumn ("Length", 2, 150, 80, 800, juce::TableHeaderComponent::notSortable);
        regionTableListBox->getHeader().setStretchToFitActive (true);
        regionTableListBox->setHeaderHeight(25);
        regionTableListBox->setOutlineThickness (0);
        regionTableListBox->updateContent();
    }

    ~RegionComponent() override
    {
        regionTableListBox->setModel(nullptr);
        regionTableListBox = nullptr;
        regionTableListBoxModel = nullptr;
    }

    void paint (juce::Graphics& g) override
    {
        /* This demo code just fills the component's background and
           draws some placeholder text to get you started.

           You should replace everything in this method with your own
           drawing code..
        */

        g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));   // clear the background

        g.setColour (juce::Colours::grey);
        g.drawRect (getLocalBounds(), 1);   // draw an outline around the component

        g.setColour (juce::Colours::white);
        g.setFont (14.0f);
        g.drawText ("RegionComponent", getLocalBounds(),
                    juce::Justification::centred, true);   // draw some placeholder text
    }

    void resized() override
    {
        // This method is where you should set the bounds of any child
        // components that your component contains..
        regionTableListBox->setBounds(0, 0, getWidth(), getHeight());
    }
    
    void updateUI()
    {
        /// If your ListBox doesn’t use custom components,
        /// then updateContents() will only update the contents if the number of rows changed, otherwise it does nothing.
        regionTableListBox->updateContent();
        
        auto selectedRow = audiumEngine->getAudioRegionContainer()->getSelectedRegion();
        regionTableListBox->selectRangeOfRows(selectedRow, selectedRow);
        
        /// repaint does the trick
        regionTableListBox->repaint();
    }
    
    void clearSelection()
    {
        regionTableListBox->deselectAllRows();
    }

private:
    std::shared_ptr<AudiumEngine> audiumEngine;
    std::shared_ptr<RegionTableListBox> regionTableListBox;
    std::unique_ptr<RegionTableListBoxModel> regionTableListBoxModel;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RegionComponent)
};
