/*
  ==============================================================================

    RegionLabel.cpp
    Created: 15 Dec 2024 12:42:47pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "TableRegionLabel.h"

void TableRegionLabel::mouseDown (const juce::MouseEvent& e)
{
    if (auto region = getRegion(columnId, rowNumber)) {
        
        if (!e.mods.isAnyModifierKeyDown() && !region->isSelected()) {
            audioTrackContainer->getSelectionManager()->deselectAll();
        }
        else {
            auto objects = audioTrackContainer->getSelectionManager()->getSelectedObjects();
            for (auto object : objects) {
                if (auto item = dynamic_cast<AudioRegion*>(object.get())) {
                    if (item->getAudioTrack() != region->getAudioTrack())
                        object->setSelected(false);
                }
            }
        }
        
        if (e.mods.isCommandDown() && region->isSelected()) {
            region->setSelected(false);
        }
        else {
            region->setSelected(true);
        }
        
        /// pass on mouse events. unless row is not selected
        getParentComponent()->mouseDown(e);
        
        // update
        audioTrackContainer->sendActionMessage(updateSelection);
    }
}

void TableRegionLabel::update(int column, int row, bool isRowSelected)
{
    columnId = column;
    rowNumber = row;
    
    juce::String text = "n/a";

    if (auto region = getRegion(columnId, rowNumber))
    {
        text = region->getName();
        
        auto textColour = region->getAudioTrack()->getColour();
        
        if (region->isSelected()) {
            setColour (juce::Label::textColourId, textColour.brighter());
            setColour (juce::Label::backgroundColourId,
                       findColour(audium::listBoxBackgroundColourId));
        }
        else {
            setColour (juce::Label::textColourId, textColour);
            setColour (juce::Label::backgroundColourId,
                       juce::Colours::transparentBlack);
        }
    }
    setText (text, juce::dontSendNotification);
}
