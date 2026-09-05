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
#include "Engine/PlayList/StretchMode.h"
#include "Engine/PlayList/PositionableBase.h"
#include "Engine/Selection/Selectable.h"
#include "Engine/Selection/SelectionManager.h"
#include "Engine/Undo/UndoableContainerAction.h"

namespace audium {

class AudioRegion;
class PlayListContainer;
class VoiceSource;

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
    
    /// The clip's timeline duration: source length / speed ratio.
    double getDurationTime(audium::TimeContextType context) const;

    /**
     * The clip's playback speed (re-pitch/varispeed): 2.0 plays double
     * speed one octave up on half the timeline. Clamped to
     * [minSpeedRatio, maxSpeedRatio]; ignored while the clip is recording.
     */
    double getSpeedRatio() const override { return speedRatio; }
    void setSpeedRatio(double newRatio);

    StretchMode getStretchMode() const { return stretchMode; }

    static constexpr double minSpeedRatio = 0.25;
    static constexpr double maxSpeedRatio = 4.0;
    
    double getAbsolutePosition(audium::TimeContextType context) const override;

    // the audible tail extension of a fade-out ending past the clip
    // (negative fadeOutEnd offset); 0.0 when the fade stays inside
    double getTailExtension(audium::TimeContextType context) const;
    void setAbsolutePosition(double position, audium::TimeContextType context) override;
    
    PlayListContainer &getPlayListContainer() const { return owner; }
    
    bool writeToJson (json& output);
    bool readFromJson (json& input, bool rebuild);
    
    bool validateData();
    
    const std::vector<std::shared_ptr<VoiceSource>> &getVoiceSources() const { return voiceSources; }
    
    void onDragStart();

    /// Commits the drag/session as one undo transaction. Safe with no
    /// preceding onDragStart (then it is a no-op).
    void onDragEnd(const juce::String& transactionName = "Set Clip Gain");

    /// Rolls the pending drag/session back to the state onDragStart
    /// captured, leaving no undo entry. Safe with no preceding
    /// onDragStart (then it is a no-op).
    void onDragCancel();

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
    std::vector<std::shared_ptr<VoiceSource>> voiceSources;
    
    std::unique_ptr<audium::UndoableContainerAction> undoableAction;
    
    // The absolute transport position
    double absolutePositionClocks = 0.0;

    // Playback speed (see getSpeedRatio) and how it is realised. Only
    // RePitch exists today.
    double speedRatio = 1.0;
    StretchMode stretchMode = StretchMode::RePitch;

    ClipDynamics dynamics{*this};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PlayListItem)
};

} // namespace audium
