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
#include "RegionTableListBoxModel.h"
#include "Interface/ColourIds.h"
#include "Interface/Controls/RegionLabel.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/PlayList/PlayListContainer.h"


RegionTableListBoxModel::RegionTableListBoxModel(std::shared_ptr<RegionTableListBox> owner,
                                                 std::shared_ptr<audium::AudioTrackContainer> audioTrackContainer) :
    owner(owner),
    audioTrackContainer(audioTrackContainer)
{
}

RegionTableListBoxModel::~RegionTableListBoxModel()
{
}

int RegionTableListBoxModel::getNumRows()
{
    return static_cast<int>(audioTrackContainer->getAudioRegionAdapter().getAudioRegions().size());
}

void RegionTableListBoxModel::paintRowBackground (juce::Graphics& g,
                                 int rowNumber,
                                 int width, int height,
                                 bool rowIsSelected)
{
    if (rowIsSelected)
    {
        g.fillAll (owner->findColour(audium::listBoxBackgroundColourId));
    }
}

void RegionTableListBoxModel::paintCell (juce::Graphics& g,
                        int rowNumber,
                        int columnId,
                        int width, int height,
                        bool rowIsSelected)
{    
}

juce::Component* RegionTableListBoxModel::refreshComponentForCell (int rowNumber, int columnId, bool isRowSelected,
                                            juce::Component* existingComponentToUpdate)
{
    auto regions = audioTrackContainer->getAudioRegionAdapter().getAudioRegions();
    if (existingComponentToUpdate == nullptr)
    {
        if (rowNumber < regions.size())
        {
            return new RegionLabel(audioTrackContainer,
                                   columnId,
                                   rowNumber);
        }
    }
    else
    {
        auto component = dynamic_cast<RegionLabel*>(existingComponentToUpdate);
        if (component != nullptr)
        {
            // update since row might have changed after delete
            component->update(columnId, rowNumber, isRowSelected);
            return component;
        }
    }
    
    return nullptr;
}

void RegionTableListBoxModel::selectedRowsChanged (int lastRowSelected)
{
    auto selectedRows = owner->getSelectedRows();
    audioTrackContainer->getAudioRegionAdapter().setSelectedRows(selectedRows);
    audioTrackContainer->sendActionMessage(audium::updateAll);
}

void RegionTableListBoxModel::backgroundClicked (const juce::MouseEvent&)
{
    owner->deselectAllRows();
}

void RegionTableListBoxModel::deleteKeyPressed (int lastRowSelected)
{
    audioTrackContainer->deleteSelectedObjects();
}

juce::var RegionTableListBoxModel::getDragSourceDescription (const juce::SparseSet<int>& currentlySelectedRows)
{
    return "region";
}

