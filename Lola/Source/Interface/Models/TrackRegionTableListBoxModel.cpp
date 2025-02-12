
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
    auto result = 0;
    for (const auto &audioTrack : audiumEngine->getAudioTrackContainer()->getAudioTracks()) {
        result = std::max(result, audioTrack->getAudioRegionContainer()->getNumRegions());
    }
    return result;
}

void TrackRegionTableListBoxModel::paintRowBackground (juce::Graphics& g,
                                 int rowNumber,
                                 int width, int height,
                                 bool rowIsSelected)
{
    if (rowIsSelected)
    {
        g.fillAll (owner->findColour(audium::listBoxBackgroundColourId));
    }
}

void TrackRegionTableListBoxModel::paintCell (juce::Graphics& g,
                        int rowNumber,
                        int columnId,
                        int width, int height,
                        bool rowIsSelected)
{    
}

juce::Component* TrackRegionTableListBoxModel::refreshComponentForCell (int rowNumber, int columnId, bool isRowSelected,
                                            juce::Component* existingComponentToUpdate)
{
    auto audioTrack = audiumEngine->getAudioTrackContainer()->getAudioTrack(columnId - 1);
    jassert(audioTrack);
    auto region = audioTrack->getAudioRegionContainer()->getRegion(rowNumber);
    
    if (existingComponentToUpdate == nullptr)
    {
        if (region != nullptr)
        {
            return new TableRegionLabel(audiumEngine->getAudioTrackContainer(),
                                        columnId,
                                        rowNumber);
        }
    }
    else
    {
        auto component = dynamic_cast<TableRegionLabel*>(existingComponentToUpdate);
        if (component != nullptr)
        {
            // update since row might have changed after delete
            component->update(columnId, rowNumber, isRowSelected);
            return component;
        }
    }
    
    return nullptr;
}

void TrackRegionTableListBoxModel::selectedRowsChanged (int lastRowSelected)
{
    auto selectedRows = owner->getSelectedRows();
    
    // TODO:
    // audioTrack->getAudioRegionContainer()->setSelectedRows(selectedRows);
    // audiumEngine->getAudioTrackContainer()->sendActionMessage(updateAll);
}

void TrackRegionTableListBoxModel::backgroundClicked (const juce::MouseEvent&)
{
    owner->deselectAllRows();
}

void TrackRegionTableListBoxModel::deleteKeyPressed (int lastRowSelected)
{
    audiumEngine->getAudioTrackContainer()->deleteSelectedObjects();
}

juce::var TrackRegionTableListBoxModel::getDragSourceDescription (const juce::SparseSet<int>& currentlySelectedRows)
{
    return "region";
}

