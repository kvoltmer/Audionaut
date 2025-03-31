//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Lola uses a GPL/commercial licence - see LICENCE.md for details.

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
                if (auto item = dynamic_cast<audium::AudioRegion*>(object.get())) {
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
        /// since this is a table we don't want the row but the cell to be selected
        ///getParentComponent()->mouseDown(e);
        
        // update
        audioTrackContainer->sendActionMessage(audium::updateSelection);
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
