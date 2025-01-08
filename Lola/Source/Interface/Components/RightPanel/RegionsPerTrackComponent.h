/*
  ==============================================================================

    RegionsPerTrackComponent.h
    Created: 14 Dec 2024 11:12:58am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "Interface/Models/RegionContainerModel.h"
#include "Interface/AudiumLookAndFeel.h"

class AudiumEngine;
class AudioTrack;

//==============================================================================
/*
*/
class RegionsPerTrackComponent  : public juce::Component
{
public:
    RegionsPerTrackComponent(std::shared_ptr<AudiumEngine> audiumEngine, std::shared_ptr<AudioTrack> track) :
        audiumEngine(audiumEngine),
        audioTrack(track)
    {
        regionListBox.reset(new juce::TableListBox());
        regionContainerModel.reset(new RegionContainerModel(regionListBox, audiumEngine, track));

        regionListBox->setModel(regionContainerModel.get());
        regionListBox->setMultipleSelectionEnabled(true);
        addAndMakeVisible(regionListBox.get());
        
        regionListBox->getHeader().addColumn ("", 1, 250, 0, 800,
                                              juce::TableHeaderComponent::notResizableOrSortable);
        regionListBox->getHeader().setStretchToFitActive (true);
        
        regionListBox->setHeaderHeight(AudiumLookAndFeel::tableHeaderHeight);
        regionListBox->setOutlineThickness (0);
        
    }
    ~RegionsPerTrackComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    
    void updateSelection();
    void updateUI();

private:
    
    std::shared_ptr<AudiumEngine> audiumEngine;
    std::shared_ptr<AudioTrack> audioTrack;
    
    std::shared_ptr<juce::TableListBox> regionListBox;
    std::unique_ptr<RegionContainerModel> regionContainerModel;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RegionsPerTrackComponent)
};
