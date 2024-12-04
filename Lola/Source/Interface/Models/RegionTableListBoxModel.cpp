/*
  ==============================================================================

    RegionTableListBoxModel.cpp
    Created: 7 Jun 2023 2:01:04pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include <JuceHeader.h>
#include "RegionTableListBoxModel.h"
#include "Interface/ColourIds.h"
#include "Interface/Controls/RegionLabel.h"
#include "Engine/Region/AudioRegionContainer.h"
#include "Engine/Group/AudioTrackContainer.h"
#include "Engine/PlayList/PlayListContainer.h"

//==============================================================================
RegionTableListBoxModel::RegionTableListBoxModel(std::shared_ptr<RegionTableListBox> owner,
                                                 std::shared_ptr<AudioTrackContainer> audioTrackContainer) :
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
            return new RegionLabel(owner, audioTrackContainer, columnId, rowNumber);
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
    audioTrackContainer->sendActionMessage(updateAll);
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
    return "region";//getParentItem()->getUniqueName() + "||" + config->getName();
}

