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

#include "TrackRegionTableListBoxModel.h"
#include "Interface/ColourIds.h"
#include "Interface/Controls/TableRegionLabel.h"
#include "Engine/AudiumEngine.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/PlayList/PlayListContainer.h"


TrackRegionTableListBoxModel::TrackRegionTableListBoxModel(std::shared_ptr<juce::TableListBox> owner_,
                                                           std::shared_ptr<AudiumEngine> audiumEngine_) :
    owner(owner_),
    audiumEngine(audiumEngine_)
{
}

TrackRegionTableListBoxModel::~TrackRegionTableListBoxModel()
{
}

int TrackRegionTableListBoxModel::getNumRows()
{
    std::size_t result = 0;
    for (const auto &audioTrack : audiumEngine->getAudioTrackContainer()->getAudioTracks()) {
        result = std::max(result, audioTrack->getRegions().size());
    }
    return static_cast<int>(result);
}

juce::Component* TrackRegionTableListBoxModel::refreshComponentForCell (int rowNumber,
                                                                        int columnId,
                                                                        bool isRowSelected,
                                                                        juce::Component* existingComponentToUpdate)
{
    auto region = getRegion(rowNumber, columnId);
    
    if (existingComponentToUpdate != nullptr) {
        std::unique_ptr<TableRegionLabel> item(dynamic_cast<TableRegionLabel*>(existingComponentToUpdate));
        if (item != nullptr && region != nullptr) {
            item->update(columnId, rowNumber, isRowSelected);
            return item.release();
        }
    }
    
    if (region != nullptr) {
        auto item = std::make_unique<TableRegionLabel>(audiumEngine->getAudioTrackContainer(),
                                                       columnId,
                                                       rowNumber);
        return item.release();
    }

    return nullptr;
}

void TrackRegionTableListBoxModel::selectedRowsChanged (int lastRowSelected)
{
}

void TrackRegionTableListBoxModel::backgroundClicked (const juce::MouseEvent&)
{
    audiumEngine->getAudioTrackContainer()->getSelectionManager()->deselectAll();
    audiumEngine->getAudioTrackContainer()->sendActionMessage(updateSelection);
}

void TrackRegionTableListBoxModel::cellClicked (int rowNumber, int columnId, const juce::MouseEvent&)
{
    if (!getRegion(rowNumber, columnId)) {
        audiumEngine->getAudioTrackContainer()->getSelectionManager()->deselectAll();
        audiumEngine->getAudioTrackContainer()->sendActionMessage(updateSelection);
    }
}

void TrackRegionTableListBoxModel::deleteKeyPressed (int lastRowSelected)
{
    audiumEngine->getAudioTrackContainer()->deleteSelectedObjects();
}

juce::var TrackRegionTableListBoxModel::getDragSourceDescription (const juce::SparseSet<int>& currentlySelectedRows)
{
    return "region";
}

std::shared_ptr<AudioRegion> TrackRegionTableListBoxModel::getRegion(int rowNumber, int columnId) const
{
    if (auto audioTrack = audiumEngine->getAudioTrackContainer()->getAudioTrack(columnId - 1))
        return audioTrack->getRegion(rowNumber);
    
    return nullptr;
}

