/*
  ==============================================================================

    SubGroupDraggerControl.cpp
    Created: 29 Nov 2024 12:00:55pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "SubGroupDraggerControl.h"

bool SubGroupDraggerControl::validateData()
{
    return audioSubGroup->getAudioClip()->validateData();
}

void SubGroupDraggerControl::shiftSelect()
{
    setSelected(true, false);
    
    // create union selection rectangle
    juce::Rectangle<int> rect;
    for (auto control : regionSelector->subGroupDraggerControls) {
        if (control->isSelected()) {
            rect = rect.getUnion(control->getScreenBounds());
        }
    }
    
    // select if it contains rectange
    for (auto control : regionSelector->subGroupDraggerControls) {
        if (rect.contains(control->getScreenBounds()) &&
            !control->isSelected()) {
            control->setSelected(true, false);
        }
    }
}
