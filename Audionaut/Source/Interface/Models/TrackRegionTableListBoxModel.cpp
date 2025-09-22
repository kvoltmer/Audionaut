//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#include <JuceHeader.h>

#include "TrackRegionTableListBoxModel.h"
#include "Interface/ColourIds.h"
#include "Interface/Controls/TableRegionLabel.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/PlayList/PlayListContainer.h"

TrackRegionTableListBoxModel::TrackRegionTableListBoxModel(std::shared_ptr<juce::TableListBox> owner_,
                                                           std::shared_ptr<audium::AudiumEngine> audiumEngine_) :
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
        auto item = std::make_unique<TableRegionLabel>(audiumEngine,
                                                       audiumEngine->getAudioTrackContainer(),
                                                       columnId,
                                                       rowNumber);
        return item.release();
    }

    return nullptr;
}

void TrackRegionTableListBoxModel::selectedRowsChanged (int lastRowSelected)
{
    auto selectedRows = owner->getSelectedRows();
    
    // selection is handled by TableRegionLabel
}

void TrackRegionTableListBoxModel::backgroundClicked (const juce::MouseEvent&)
{
    audiumEngine->getAudioTrackContainer()->getSelectionManager()->deselectAll();
    audiumEngine->getAudioTrackContainer()->sendActionMessage(audium::updateSelection);
}

void TrackRegionTableListBoxModel::cellClicked (int rowNumber, int columnId, const juce::MouseEvent&)
{
    if (!getRegion(rowNumber, columnId)) {
        audiumEngine->getAudioTrackContainer()->getSelectionManager()->deselectAll();
        audiumEngine->getAudioTrackContainer()->sendActionMessage(audium::updateSelection);
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

std::shared_ptr<audium::AudioRegion> TrackRegionTableListBoxModel::getRegion(int rowNumber, int columnId) const
{
    if (auto audioTrack = audiumEngine->getAudioTrackContainer()->getAudioTrack(columnId - 1))
        return audioTrack->getRegion(rowNumber);
    
    return nullptr;
}

