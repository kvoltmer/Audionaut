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
#include "Interface/Controls/RegionNameTextEditor.h"

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
        g.fillAll (owner->findColour(defaultHighlightColourId));
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
            g.setColour (owner->findColour (defaultHighlightedTextColourId));
        else
            g.setColour (owner->findColour (defaultTextColourId));

        g.setFont (13.0f);
        g.drawText (text, 4, 0, width - 6, height, juce::Justification::centredLeft, true);
    }
    
}

//juce::Component* RegionTableListBoxModel::refreshComponentForCell (int rowNumber, int columnId, bool isRowSelected,
//                                            juce::Component* existingComponentToUpdate)
//{
//    if (existingComponentToUpdate == nullptr)
//    {
//        if (const AudioRegion* const r = audioRegionContainer->getRegion(rowNumber).get())
//        {
//            if (columnId == 0)
//            {
//                return new RegionNameTextEditor(audioRegionContainer->getRegion(rowNumber));
//            }
//            else
//            {
//                return new juce::TextEditor();
//            }
//        }
//    }
//    else
//    {
//        if (columnId == 0)
//        {
//            auto component = dynamic_cast<RegionNameTextEditor*>(existingComponentToUpdate);
//            jassert(component);
//            if (const AudioRegion* const r = audioRegionContainer->getRegion(rowNumber).get())
//            {
//                // update since row might have changed after delete
//                //component->
//            }
//            return component;
//        }
//        else
//        {
//            auto component = dynamic_cast<juce::TextEditor*>(existingComponentToUpdate);
//            jassert(component);
//            if (const AudioRegion* const r = audioRegionContainer->getRegion(rowNumber).get())
//            {
//                // update since row might have changed after delete
//                //component->
//            }
//            return component;
//        }
//    }
//    
//    return nullptr;
//}

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

void RegionTableListBoxModel::returnKeyPressed (int lastRowSelected)
{
    
}
