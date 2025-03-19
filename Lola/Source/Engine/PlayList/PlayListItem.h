//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Lola uses a GPL/commercial licence - see LICENCE.md for details.

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
#include "Engine/Undo/UndoableContainerAction.h"

namespace audium {

class AudioRegion;
class PlayListContainer;
class AudiumTransportSource;

class PlayListItem : public PositionableBase, public audium::Selectable
{
    
public:
    
    PlayListItem(const PlayListContainer &owner,
                 std::shared_ptr<AudioRegion> audioRegion,
                 std::shared_ptr<SelectionManager> selectionManager);
    
    ~PlayListItem() override;
    
    void init();
    void deinit();
    
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
    
    void onDragStart();
    void onDragEnd();
    
    // value range [0, 1]
    bool setFadeIn(double val);
    double getFadeIn() const;
    
    // value range [0, 1]
    bool setFadeOut(double val);
    double getFadeOut() const;
    
    double getFadeInClocks() const
    {
        return fadeInClocks;
    }
    
    double getFadeOutClocks() const
    {
        return fadeOutClocks;
    }
    
private:
    const PlayListContainer &owner;
    std::shared_ptr<AudioRegion> audioRegion;
    std::vector<std::shared_ptr<AudiumTransportSource>> transportSources;
    
    std::unique_ptr<audium::UndoableContainerAction> undoableAction;
    
    // The absolute transport position
    double absolutePositionClocks = 0.0;
    
    double gain = 1.0;
    double fadeInClocks = 0.0;
    double fadeOutClocks = 0.0;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListItem)
};

} // namespace audium
