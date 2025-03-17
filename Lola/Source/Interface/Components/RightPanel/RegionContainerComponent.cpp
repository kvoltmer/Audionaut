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

#include <JuceHeader.h>
#include "RegionContainerComponent.h"
#include "Interface/Models/TrackRegionTableListBoxModel.h"
#include "Interface/LookAndFeel/AudiumLookAndFeel.h"

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
