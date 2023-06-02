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
    
    std::shared_ptr<AudioRegion> createRegion(juce::String regionName);
    
    void setSelectedRegion(juce::Range<double> pos);
    juce::Range<double> getSelectedRegion() const;
        
    bool writeToStream (juce::OutputStream& outputStream);
    bool readFromStream (juce::InputStream& inputStream);
    
private:
    AudioRegion selectedRegion;
    
    std::vector<std::shared_ptr<AudioRegion>> audioRegions;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioRegionContainer)
};
