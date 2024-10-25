/*
  ==============================================================================

    DspClipData.h
    Created: 31 May 2024 11:42:19am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include "Engine/Group/AudioClipData.h"



struct DspClipData
{
    // The audio clip data
    AudioClipData clipData;
    
    // Indicate if clip is active
    bool active = false;
    
    float gain = 1.f;
    
    int transportSourceIndex = -1;

};
