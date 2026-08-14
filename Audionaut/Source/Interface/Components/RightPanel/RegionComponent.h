//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>

#include "Engine/AudiumEngine.h"
#include "Engine/PlayList/PlayListItem.h"
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
    RegionComponent(std::shared_ptr<audium::AudiumEngine> audiumEngine) :
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
        auto audioTrackContainer = audiumEngine->getAudioTrackContainer();
        auto selection = audioTrackContainer->getAudioRegionAdapter().getSelectedRows();

        // regions referenced by selected playlist items highlight too, so
        // clicking an item points at its source region. Display only - the
        // region is not engine-selected, so delete/copy still target the item.
        auto regions = audioTrackContainer->getAudioRegionAdapter().getAudioRegions();
        for (const auto& object : audioTrackContainer->getSelectionManager()->getSelectedObjects())
            if (auto* item = dynamic_cast<audium::PlayListItem*>(object.get()))
                for (int row = 0; row < static_cast<int>(regions.size()); row++)
                    if (regions[static_cast<size_t>(row)] == item->getRegion())
                        selection.addRange({row, row + 1});

        regionTableListBox->setSelectedRows(selection, juce::dontSendNotification);
    }
    
    void updateUI(UIContext context)
    {
        /// TODO: implement UI context
        updateSelection();
        regionTableListBox->updateContent();
    }

private:
    std::shared_ptr<audium::AudiumEngine> audiumEngine;
    std::shared_ptr<RegionTableListBox> regionTableListBox;
    std::unique_ptr<RegionTableListBoxModel> regionTableListBoxModel;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RegionComponent)
};
