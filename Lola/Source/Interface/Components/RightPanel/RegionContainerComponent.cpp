
#include <JuceHeader.h>
#include "RegionContainerComponent.h"
#include "Interface/Models/TrackRegionTableListBoxModel.h"
#include "Interface/AudiumLookAndFeel.h"

#include "Engine/AudiumEngine.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Group/AudioTrack.h"

RegionContainerComponent::RegionContainerComponent(std::shared_ptr<AudiumEngine> audiumEngine_) :
    audiumEngine(audiumEngine_)
{
    regionTableListBox.reset(new juce::TableListBox());
    regionTableListBoxModel.reset(new TrackRegionTableListBoxModel(regionTableListBox, audiumEngine));

    regionTableListBox->setModel(regionTableListBoxModel.get());
    regionTableListBox->setMultipleSelectionEnabled(true);
    addAndMakeVisible(regionTableListBox.get());
    
//    regionTableListBox->getHeader().addColumn ("", 1, 250, 0, 800,
//                                          juce::TableHeaderComponent::notResizableOrSortable);
    regionTableListBox->getHeader().setStretchToFitActive (true);
    
    regionTableListBox->setHeaderHeight(AudiumLookAndFeel::tableHeaderHeight);
    regionTableListBox->setOutlineThickness (0);
}

RegionContainerComponent::~RegionContainerComponent()
{
    regionTableListBox->setModel(nullptr);
    regionTableListBox = nullptr;
    regionTableListBoxModel = nullptr;
}

void RegionContainerComponent::paint (juce::Graphics& g)
{
}

void RegionContainerComponent::resized()
{
    regionTableListBox->setBounds(getLocalBounds());

}

void RegionContainerComponent::updateUI(UIContext context)
{
    //updateSelection();
    
    if (context == RebuildContext) {
        regionTableListBox->getHeader().removeAllColumns();
        for (const auto &audioTrack : audiumEngine->getAudioTrackContainer()->getAudioTracks()) {
            regionTableListBox->getHeader().addColumn (audioTrack->getAudioTrackName(),
                                                       audioTrack->getId() + 1,
                                                       250,
                                                       0,
                                                       800,
                                                       juce::TableHeaderComponent::notResizableOrSortable);
        }
        
    }
    regionTableListBox->updateContent();
    
    //regionListBox->getHeader().setColour(juce::TableHeaderComponent::textColourId, audioTrack->getColour());
    //regionListBox->getHeader().setColumnName(1, audioTrack->getAudioTrackName());
    
}

void RegionContainerComponent::clearSelection()
{
    
}
