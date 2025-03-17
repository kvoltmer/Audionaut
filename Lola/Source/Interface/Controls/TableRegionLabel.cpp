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
