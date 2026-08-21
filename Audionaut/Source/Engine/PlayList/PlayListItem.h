//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <vector>
#include <memory>
#include <JuceHeader.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

#include "Engine/TimeContext.h"
#include "Engine/PlayList/ClipDynamics.h"
#include "Engine/PlayList/PositionableBase.h"
#include "Engine/Selection/Selectable.h"
#include "Engine/Selection/SelectionManager.h"
#include "Engine/Undo/UndoableContainerAction.h"

namespace audium {

class AudioRegion;
class PlayListContainer;
class AudiumTransportSource;

/**
 * \class PlayListItem
 * \brief Represents an item in a playlist, managing its position, region, and associated data.
 *
 * The `PlayListItem` class is responsible for handling individual items in a playlist.
 * It provides functionality for managing the item's position, associated audio region,
 * fades, and transport sources. It also supports serialization to and from JSON.
 */
class PlayListItem : public PositionableBase, public audium::Selectable
{
    
public:
    
    PlayListItem(PlayListContainer &owner,
                 std::shared_ptr<AudioRegion> audioRegion,
                 std::shared_ptr<SelectionManager> selectionManager);
    
    ~PlayListItem() override;
    
    void init();
    void deinit();
    void createTransportSources();
    
    void cleanup() override {}
    
    std::shared_ptr<AudioRegion> getRegion() const { return audioRegion; }
    
    juce::Range<double> getRegionData(audium::TimeContextType context) const override;
    void setRegionData(juce::Range<double> newRegionData, audium::TimeContextType context) override;
    
    double getDurationTime(audium::TimeContextType context) const;
    
    double getAbsolutePosition(audium::TimeContextType context) const override;

    // the audible tail extension of a fade-out ending past the clip
    // (negative fadeOutEnd offset); 0.0 when the fade stays inside
    double getTailExtension(audium::TimeContextType context) const;
    void setAbsolutePosition(double position, audium::TimeContextType context) override;
    
    PlayListContainer &getPlayListContainer() const { return owner; }
    
    bool writeToJson (json& output);
    bool readFromJson (json& input, bool rebuild);
    
    bool validateData();
    
    const std::vector<std::shared_ptr<AudiumTransportSource>> &getTransportSources() const { return transportSources; }
    
    void onDragStart();
    void onDragEnd();

    ClipDynamics& getDynamics() { return dynamics; }
    const ClipDynamics& getDynamics() const { return dynamics; }

    bool isRecording() const;
    
    const double getRecordedLength(audium::TimeContextType context) const;
    
    const double getRecordingStartPosition(audium::TimeContextType context) const;
    
    int getId() const;
    
    // Recording helpers:
    bool needsLengthUpdate = false;
    bool isFirstPartInLoop = false;
    bool isSecondPartInLoop = false;
    
private:
    PlayListContainer &owner;
    std::shared_ptr<AudioRegion> audioRegion;
    std::vector<std::shared_ptr<AudiumTransportSource>> transportSources;
    
    std::unique_ptr<audium::UndoableContainerAction> undoableAction;
    
    // The absolute transport position
    double absolutePositionClocks = 0.0;

    ClipDynamics dynamics{*this};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListItem)
};

} // namespace audium
