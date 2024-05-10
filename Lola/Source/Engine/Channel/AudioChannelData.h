/*
  ==============================================================================

    AudioChannelData.h
    Created: 26 Feb 2024 11:45:46am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <nlohmann/json.hpp>

using json = nlohmann::json;

//==============================================================================
struct AudioChannelData
{
    int height = 100;
    bool selected = false;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(AudioChannelData, height, selected);


