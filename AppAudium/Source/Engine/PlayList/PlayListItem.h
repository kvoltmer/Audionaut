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

#include "Engine/TimeContext.h"
#include "Engine/PlayList/PositionableBase.h"

class AudioRegion;
class PlayListContainer;

class PlayListItem : public PositionableBase
{
    
public:
    
    PlayListItem(const PlayListContainer &owner, std::shared_ptr<AudioRegion> audioRegion);
        
    std::shared_ptr<AudioRegion> getRegion() const { return audioRegion; }
    
    juce::Range<double> getRegionData(audium::TimeContextType context) const;
    
    double getAbsolueStartTime(audium::TimeContextType context) const;
    double getDurationTime(audium::TimeContextType context) const;
    
    void setSelected(bool bSelected) { selected = bSelected; }
    bool isSelected() const { return selected; }
    
    juce::Range<double> getAbsolutePosition(audium::TimeContextType context) const override;

private:
    const PlayListContainer &owner;
    std::shared_ptr<AudioRegion> audioRegion;
    bool selected = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListItem)
};
