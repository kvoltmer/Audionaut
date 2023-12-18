/*
  ==============================================================================

    PlayListItem.h
    Created: 28 Jun 2023 11:51:10am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include <vector>
#include <memory>
#include <JuceHeader.h>

class AudioRegion;
class PlayListContainer;

class PlayListItem
{
    
public:
    
    PlayListItem(const PlayListContainer &owner, std::shared_ptr<AudioRegion> audioRegion);
    
    std::shared_ptr<AudioRegion> getRegion() const { return audioRegion; }
    
    juce::Range<double> getRegionDataInClocks() const;
    
    double getAbsolueStartTime() const;
    double getDurationTimeInClocks() const;
    
    void setSelected(bool bSelected) { selected = bSelected; }
    bool isSelected() const { return selected; }
    
private:
    const PlayListContainer &owner;
    std::shared_ptr<AudioRegion> audioRegion;
    bool selected = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListItem)
};
