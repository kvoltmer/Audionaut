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

