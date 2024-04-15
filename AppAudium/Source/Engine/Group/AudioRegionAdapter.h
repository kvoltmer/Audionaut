/*
  ==============================================================================

    AudioRegionAdapter.h
    Created: 15 Apr 2024 11:06:02am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>

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

private:
    AudioGroupContainer &owner;
};
