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
#include <nlohmann/json.hpp>

using json = nlohmann::json;

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
    void setRegionData(juce::Range<double> newRegionData, audium::TimeContextType context);
    
    double getDurationTime(audium::TimeContextType context) const;
    
    void setSelected(bool bSelected) { selected = bSelected; }
    bool isSelected() const { return selected; }
    
    juce::Range<double> getAbsolutePositionRange(audium::TimeContextType context) const override;
    
    void setAbsoluteStartPosition(double newStart, audium::TimeContextType context) override;
    void setLength(double newLength, audium::TimeContextType context) override;
    
    double getAbsolutePosition(audium::TimeContextType context) const override;
    void setAbsolutePosition(double position, audium::TimeContextType context) override;
    
    void moveAbsolutePosition(double amount, audium::TimeContextType context);
    
    const PlayListContainer &getPlayListContainer() const { return owner; }

    bool writeToJson (json& output);
    bool readFromJson (json& input, bool rebuild);
    
    bool validateData();

private:
    const PlayListContainer &owner;
    std::shared_ptr<AudioRegion> audioRegion;
    bool selected = false;
    
    // The absolute transport position
    double absolutePositionClocks = 0.0;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListItem)
};
