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
#include "Engine/Selection/Selectable.h"
#include "Engine/Selection/SelectionManager.h"

class AudioRegion;
class PlayListContainer;
class AudiumTransportSource;

class PlayListItem : public PositionableBase, public audium::Selectable
{
    
public:
    
    PlayListItem(const PlayListContainer &owner,
                 std::shared_ptr<AudioRegion> audioRegion,
                 std::shared_ptr<audium::SelectionManager> selectionManager);
    
    ~PlayListItem() override;
    
    void cleanup() override {}
    
    std::shared_ptr<AudioRegion> getRegion() const { return audioRegion; }
    
    juce::Range<double> getRegionData(audium::TimeContextType context) const override;
    void setRegionData(juce::Range<double> newRegionData, audium::TimeContextType context) override;
    
    double getDurationTime(audium::TimeContextType context) const;
            
    double getAbsolutePosition(audium::TimeContextType context) const override;
    void setAbsolutePosition(double position, audium::TimeContextType context) override;
        
    const PlayListContainer &getPlayListContainer() const { return owner; }

    bool writeToJson (json& output);
    bool readFromJson (json& input, bool rebuild);
    
    bool validateData();

    const std::vector<std::shared_ptr<AudiumTransportSource>> &getTransportSources() const { return transportSources; }
    
private:
    const PlayListContainer &owner;
    std::shared_ptr<AudioRegion> audioRegion;
    std::vector<std::shared_ptr<AudiumTransportSource>> transportSources;
    
    // The absolute transport position
    double absolutePositionClocks = 0.0;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListItem)
};
