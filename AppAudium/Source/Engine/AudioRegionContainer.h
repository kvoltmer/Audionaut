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

class AudioResourceContainer;

class AudioRegionContainer : public juce::ActionBroadcaster
{
                                            
public:
    AudioRegionContainer() = default;
    
    std::shared_ptr<AudioRegion> createDefaultRegion(std::shared_ptr<AudioResourceContainer> audioResourceContainer);
    std::shared_ptr<AudioRegion> createRegion(juce::String regionName, juce::Range<double> position);
    void deleteRegion(int rowNumber);
    
    void setRegionPosition(juce::Range<double> pos);
    juce::Range<double> getRegionPosition() const;
        
    bool writeToStream (juce::OutputStream& outputStream);
    bool readFromStream (juce::InputStream& inputStream);
    
    int getNumRegions() const { return static_cast<int>(audioRegions.size()); }
    std::shared_ptr<AudioRegion> getRegion(int index) const;
    int getRegionIndex(std::shared_ptr<AudioRegion> searchRegion) const;
    
    void setSelectedRegion(int rowNumber);
    int getSelectedRegion() const;
    void clearSelection();
    
    void setRegionName(int rowNumber, juce::String newName);
    void setRegionStart(int rowNumber, double newStart);
    void setRegionEnd(int rowNumber, double newEnd);
    void setRegionLength(int rowNumber, double newLength);
    
    void cleanup() { audioRegions.clear(); }
    
private:
    AudioRegion selectedRegion;
    int selectedRowNumber = -1;
    
    std::vector<std::shared_ptr<AudioRegion>> audioRegions;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioRegionContainer)
};
