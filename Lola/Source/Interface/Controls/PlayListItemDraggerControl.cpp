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

#include "PlayListItemDraggerControl.h"

bool PlayListItemDraggerControl::validateData()
{
    bool result = playListItem->validateData();
    // sort by position
    playListContainer->sortByPosition();
    return result;
}


void PlayListItemDraggerControl::shiftSelect()
{
    setSelected(true, false);
    
    // create union selection rectangle
    juce::Rectangle<int> rect;
    for (auto control : regionSelector->playListItemDraggerControls) {
        if (control->isSelected()) {
            rect = rect.getUnion(control->getScreenBounds());
        }
    }
    
    // select if it contains rectange
    for (auto control : regionSelector->playListItemDraggerControls) {
        if (rect.contains(control->getScreenBounds()) &&
            !control->isSelected()) {
            control->setSelected(true, false);
        }
    }
}
