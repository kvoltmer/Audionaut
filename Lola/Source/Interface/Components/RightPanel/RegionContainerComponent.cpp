//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Lola uses a GPL/commercial licence - see LICENCE.md for details.

#include <JuceHeader.h>
#include "RegionContainerComponent.h"
#include "Interface/Models/TrackRegionTableListBoxModel.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"

#include "Engine/AudiumEngine.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/Group/AudioTrack.h"

RegionContainerComponent::RegionContainerComponent(std::shared_ptr<audium::AudiumEngine> audiumEngine_) :
    audiumEngine(audiumEngine_)
{
    regionTableListBox.reset(new juce::TableListBox());
    regionTableListBoxModel.reset(new TrackRegionTableListBoxModel(regionTableListBox, audiumEngine));

    regionTableListBox->setModel(regionTableListBoxModel.get());
    regionTableListBox->setMultipleSelectionEnabled(true);
    addAndMakeVisible(regionTableListBox.get());
    regionTableListBox->getHeader().setStretchToFitActive (true);
    
    regionTableListBox->setHeaderHeight(AudiumLookAndFeel::tableHeaderHeight);
    regionTableListBox->setOutlineThickness (0);
    regionTableListBox->setLookAndFeel(&lookAndFeel);
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
    auto tracks = audiumEngine->getAudioTrackContainer()->getAudioTracks();

    
    if (context == RebuildContext ||
        tracks.size() != regionTableListBox->getHeader().getNumColumns(true)) {
        regionTableListBox->getHeader().removeAllColumns();
        for (auto track : tracks) {
            regionTableListBox->getHeader().addColumn (track->getAudioTrackName(),
                                                       track->getId() + 1,
                                                       250,
                                                       0,
                                                       800,
                                                       juce::TableHeaderComponent::notResizableOrSortable);
        }
    }
    
    for (auto track : tracks) {
        regionTableListBox->getHeader().setColumnName(track->getId() + 1,
                                                      track->getAudioTrackName());
    }
    
    regionTableListBox->updateContent();
}
