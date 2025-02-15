/*
  ==============================================================================

    AudioResourceData.h
    Created: 23 Feb 2024 3:49:15pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

//==============================================================================
struct AudioResourceData
{
    typedef class juce::Range<double> tRange;
    
    std::string url;
    float gain;
    int channelPos;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(AudioResourceData,
                                   url,
                                   gain,
                                   channelPos);

