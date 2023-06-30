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
#include "Interface/Controls/RegionTableListBox.h"
#include "Interface/Controls/RegionEditor.h"

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
        g.fillAll (owner->findColour(audium::defaultHighlightColourId));
}

void RegionTableListBoxModel::paintCell (juce::Graphics& g,
                        int rowNumber,
                        int columnId,
                        int width, int height,
                        bool rowIsSelected)
{
    if (const AudioRegion* const r = audioRegionContainer->getRegion(rowNumber).get())
    {
        juce::String text;

        if (columnId == 1)
        {
            text = r->name;
        }
        else if (columnId == 2)
        {
            text = juce::String(r->position.getLength(), 2);
        }
        
        if (rowIsSelected)
            g.setColour (owner->findColour (audium::defaultHighlightedTextColourId));
        else
            g.setColour (owner->findColour (audium::defaultTextColourId));

        g.setFont (13.0f);
        g.drawText (text, 4, 0, width - 6, height, juce::Justification::centredLeft, true);
    }
    
}

juce::Component* RegionTableListBoxModel::refreshComponentForCell (int rowNumber, int columnId, bool isRowSelected,
                                            juce::Component* existingComponentToUpdate)
{
    if (existingComponentToUpdate == nullptr)
    {
        if (const AudioRegion* const r = audioRegionContainer->getRegion(rowNumber).get())
        {
            return new RegionEditor(owner, audioRegionContainer, columnId, rowNumber);
        }
    }
    else
    {
        auto component = dynamic_cast<RegionEditor*>(existingComponentToUpdate);
        if (component != nullptr)
        {
            // update since row might have changed after delete
            component->update(columnId, rowNumber, isRowSelected);
        }
        return component;

    }
    
    return nullptr;
}

void RegionTableListBoxModel::selectedRowsChanged (int lastRowSelected)
{
    audioRegionContainer->setSelectedRegion(lastRowSelected);
}

void RegionTableListBoxModel::deleteKeyPressed (int lastRowSelected)
{
    auto selected = owner->getSelectedRows();
    
    for (int i = selected.size()-1; i >= 0; i--)
    {
        audioRegionContainer->deleteRegion(selected[i]);
    }
}

juce::var RegionTableListBoxModel::getDragSourceDescription (const juce::SparseSet<int>& currentlySelectedRows)
{
    return "region";//getParentItem()->getUniqueName() + "||" + config->getName();
}

