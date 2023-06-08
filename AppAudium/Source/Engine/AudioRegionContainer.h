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

const char* const regionCreatedAction   = "region created";
const char* const regionClearedAction   = "region cleared";
const char* const regionModifiedAction  = "region modified";
const char* const regionSelectedAction  = "region selected";

class AudioRegionContainer : public juce::ActionBroadcaster
{
                                            
public:
    AudioRegionContainer() = default;
    
    void createRegion(juce::String regionName, juce::Range<double> position);
    
    void setRegionPosition(juce::Range<double> pos);
    juce::Range<double> getRegionPosition() const;
        
    bool writeToStream (juce::OutputStream& outputStream);
    bool readFromStream (juce::InputStream& inputStream);
    
    int getNumRegions() const { return static_cast<int>(audioRegions.size()); }
    AudioRegion* getRegion(int index) const;
    
    void setSelectedRegion(int rowNumber);
    int getSelectedRegion() const;
    void clearSelection();
    
private:
    AudioRegion selectedRegion;
    int selectedRowNumber = -1;
    
    std::vector<std::shared_ptr<AudioRegion>> audioRegions;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioRegionContainer)
};
