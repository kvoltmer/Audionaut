/*
  ==============================================================================

    AudioRegionAdapter.h
    Created: 15 Apr 2024 11:06:02am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>

#include "Engine/TimeContext.h"
#include "Engine/Region/AudioRegionData.h"

class AudioGroupContainer;
class AudioRegion;

class AudioRegionAdapter
{
    
public:
    
    AudioRegionAdapter(AudioGroupContainer &owner);
    
    const std::vector<std::shared_ptr<AudioRegion>> getAudioRegions() const;
    const std::vector<std::shared_ptr<AudioRegion>> getSelectedAudioRegions() const;
    
    void deselectAll();
    
    juce::SparseSet<int> getSelectedRows() const;
    void setSelectedRows(juce::SparseSet<int>& selectedRows);

    void deleteSelectedRegions();
    
    void createRegionsFromSelection(juce::String name, bool arrangementMode);
    void setSelectedPosition(juce::Range<double> pos, audium::TimeContextType context);
    juce::Range<double> getSelectedPosition(audium::TimeContextType context) const;

private:
    AudioGroupContainer &owner;
    
    AudioRegionData::tRange selectedPositionClocks;
};
