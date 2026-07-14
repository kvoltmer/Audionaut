//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <set>

#include <JuceHeader.h>
#include <nlohmann/json.hpp>
#include "Engine/Analysis/AnalysisCache.h"

using json = nlohmann::json;

namespace audium {

class AudioTrack;

/**
 * @class AudioTrackViewState
 * @brief Encapsulates the view/display state of an `AudioTrack`.
 *
 * Holds everything the UI shows for a track but the audio engine does not need:
 * track colour, minimized state, visible analysis overlays and channel heights.
 * The state is persisted as part of the owning track's JSON.
 */
class AudioTrackViewState
{

public:
    /**
     * @brief Constructs an `AudioTrackViewState` instance.
     * @param track_ Reference to the owning `AudioTrack`.
     */
    explicit AudioTrackViewState(AudioTrack& track_) : track(track_) {}

    /**
     * @brief Sets the color of the audio track.
     * @param colour The new color.
     */
    void setColour(juce::Colour colour) { trackColour = colour; }

    /**
     * @brief Gets the color of the audio track.
     * @return The current color.
     */
    juce::Colour getColour() const { return trackColour; }

    bool getMinimized() const { return isMinimized; }
    void setMinimized(bool minimized) { isMinimized = minimized; }

    // Which analysis types (SBic/onset/beat) are shown in this track's clips.
    bool isAnalysisTypeVisible(AnalysisType type) const { return visibleAnalysisTypes.count(type) > 0; }
    void setAnalysisTypeVisible(AnalysisType type, bool visible)
    {
        if (visible)
            visibleAnalysisTypes.insert(type);
        else
            visibleAnalysisTypes.erase(type);
    }
    const std::set<AnalysisType>& getVisibleAnalysisTypes() const { return visibleAnalysisTypes; }

    // channel height:
    int getTotalHeight() const;
    void setChannelHeight(int height);

    // serialization (called from AudioTrack::writeToJson / readFromJson):
    bool writeToJson (json& output) const;
    bool readFromJson (json& input);

private:
    AudioTrack& track; ///< Reference to the owning audio track.

    juce::Colour trackColour = juce::Colours::pink; ///< Color of the audio track.
    bool isMinimized = false;

    // Analysis overlays visible in this track's clips (none shown by default).
    std::set<AnalysisType> visibleAnalysisTypes;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioTrackViewState)

};

} // namespace audium
