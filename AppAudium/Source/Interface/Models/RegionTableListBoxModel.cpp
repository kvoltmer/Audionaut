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
#include "Engine/PlayList/PlayListContainer.h"

//==============================================================================
RegionTableListBoxModel::RegionTableListBoxModel(std::shared_ptr<RegionTableListBox> owner,
                                                 std::shared_ptr<AudioRegionContainer> audioRegionContainer) :
    owner(owner),
    audioRegionContainer(audioRegionContainer)
{
}

RegionTableListBoxModel::~RegionTableListBoxModel()
{
}

int RegionTableListBoxModel::getNumRows()
{
    return audioRegionContainer->getNumRegions();
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
    if (existingComponentToUpdate == nullptr)
    {
        if (const AudioRegion* const r = audioRegionContainer->getRegion(rowNumber).get())
        {
            return new RegionLabel(owner, audioRegionContainer, columnId, rowNumber);
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
    audioRegionContainer->setSelectedRows(selectedRows);
    audioRegionContainer->sendActionMessage(updateMiddlePanelAction);
}

void RegionTableListBoxModel::backgroundClicked (const juce::MouseEvent&)
{
    owner->deselectAllRows();
}

void RegionTableListBoxModel::deleteKeyPressed (int lastRowSelected)
{
    audioRegionContainer->deleteSelectedRegions();
}

juce::var RegionTableListBoxModel::getDragSourceDescription (const juce::SparseSet<int>& currentlySelectedRows)
{
    return "region";//getParentItem()->getUniqueName() + "||" + config->getName();
}

