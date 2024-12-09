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

class AudioTrackContainer;
class AudioRegion;

class AudioRegionAdapter
{
    
public:
    
    AudioRegionAdapter(AudioTrackContainer &owner);
    ~AudioRegionAdapter() = default;
    
    const std::vector<std::shared_ptr<AudioRegion>> getAudioRegions() const;
    const std::vector<std::shared_ptr<AudioRegion>> getSelectedAudioRegions() const;
    std::shared_ptr<AudioRegion> getRegion(int rowNumber) const;
    
    void deselectAll();
    
    juce::SparseSet<int> getSelectedRows() const;
    void setSelectedRows(juce::SparseSet<int>& selectedRows);
    
    void createRegionsFromSelection(juce::String name, bool arrangementMode);
    void splitRegionsFromSelection(bool withUndo = true);
    
    void setSelectedRange(juce::Range<double> pos, audium::TimeContextType context);
    juce::Range<double> getSelectedRange(audium::TimeContextType context) const;
    bool anyRangeSelected() const;

private:
    AudioTrackContainer &owner;
    
    AudioRegionData::tRange selectedPositionClocks;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioRegionAdapter)
};
