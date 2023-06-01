/*
  ==============================================================================

    AudioRegionContainer.h
    Created: 30 May 2023 10:16:35am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include "Engine/AudioRegion.h"

class AudioRegionContainer
{
                                            
public:
    AudioRegionContainer() = default;
    
    void createRegion(juce::String regionName);
    
    void setSelectedRegion(juce::Range<double> pos);
    juce::Range<double> getSelectedRegion() const;
        
private:
    AudioRegion selectedRegion;
    
    std::vector<std::shared_ptr<AudioRegion>> audioRegions;
    
};
