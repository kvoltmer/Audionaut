//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <JuceHeader.h>

#include "Engine/AudiumEngine.h"
#include "Interface/Models/RegionTableListBoxModel.h"
#include "Interface/Controls/RegionTableListBox.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"

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
        regionTableListBoxModel.reset(new RegionTableListBoxModel(regionTableListBox,
                                                                  audiumEngine->getAudioTrackContainer()));

        regionTableListBox->setModel(regionTableListBoxModel.get());
        regionTableListBox->setMultipleSelectionEnabled(true);
        addAndMakeVisible(regionTableListBox.get());
        
        regionTableListBox->getHeader().addColumn ("Name", regionName, 250, 80, 800, juce::TableHeaderComponent::notSortable);
        regionTableListBox->getHeader().addColumn ("Start", regionStart, 250, 80, 800, juce::TableHeaderComponent::notSortable);
        regionTableListBox->getHeader().addColumn ("End", regionEnd, 250, 80, 800, juce::TableHeaderComponent::notSortable);
        regionTableListBox->getHeader().addColumn ("Length", regionLength, 150, 80, 800, juce::TableHeaderComponent::notSortable);
        regionTableListBox->getHeader().setStretchToFitActive (true);
        regionTableListBox->setHeaderHeight(AudiumLookAndFeel::tableHeaderHeight);
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
        g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

        g.setColour (juce::Colours::grey);
        g.drawRect (getLocalBounds(), 1);   // draw an outline around the component

        g.setColour (juce::Colours::white);
        g.setFont (14.0f);
        g.drawText ("RegionComponent", getLocalBounds(),
                    juce::Justification::centred, true);   // draw some placeholder text
    }

    void resized() override
    {
        regionTableListBox->setBounds(getLocalBounds());
    }
    
    void updateSelection()
    {
        auto selection = audiumEngine->getAudioTrackContainer()->getAudioRegionAdapter().getSelectedRows();
        regionTableListBox->setSelectedRows(selection, juce::dontSendNotification);
    }
    
    void updateUI(UIContext context)
    {
        /// TODO: implement UI context
        updateSelection();
        regionTableListBox->updateContent();
    }

private:
    std::shared_ptr<AudiumEngine> audiumEngine;
    std::shared_ptr<RegionTableListBox> regionTableListBox;
    std::unique_ptr<RegionTableListBoxModel> regionTableListBoxModel;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RegionComponent)
};
