//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include "Engine/TimeContext.h"
#include "Engine/PlayList/PositionableBase.h"
#include "Engine/Core/DspClipData.h"
#include "Engine/Provider/TempoProvider.h"

namespace audium {

/**
 * @class DspClip
 * @brief Represents a digital signal processing (DSP) clip in the Audionaut application.
 *
 * The `DspClip` class provides functionality to manage and manipulate DSP clip data,
 * including its position and region within a timeline. It inherits from `PositionableBase`
 * to support positioning in different time contexts.
 */
class DspClip : public PositionableBase
{
public:
    /**
     * @brief Constructs a `DspClip` with a tempo provider and initial clip data.
     * @param tempoProvider_ A shared pointer to the `TempoProvider` for tempo-related calculations.
     * @param data The initial `DspClipData` associated with this clip.
     */
    DspClip(std::shared_ptr<TempoProvider> tempoProvider_, DspClipData data_) :
        tempoProvider(tempoProvider_),
        dspClipData(data_)
    {}

    /**
     * @brief Retrieves the region data of the clip in a specific time context.
     * @param context The time context in which to retrieve the region data.
     * @return A `juce::Range<double>` representing the region data.
     */
    juce::Range<double> getRegionData(audium::TimeContextType context) const override;

    /**
     * @brief Sets the region data of the clip in a specific time context.
     * @param newRegionData The new region data to set.
     * @param context The time context in which to set the region data.
     */
    void setRegionData(juce::Range<double> newRegionData, audium::TimeContextType context) override;

    /// The clip's playback speed, straight from the committed clip data.
    double getSpeedRatio() const override { return dspClipData.clipSpeedRatio; }

    /**
     * @brief Retrieves the absolute position of the clip in a specific time context.
     * @param context The time context in which to retrieve the position.
     * @return The absolute position as a `double`.
     */
    double getAbsolutePosition(audium::TimeContextType context) const override;

    /**
     * @brief Sets the absolute position of the clip in a specific time context.
     * @param position The new absolute position to set.
     * @param context The time context in which to set the position.
     */
    void setAbsolutePosition(double position, audium::TimeContextType context) override;

    /**
     * @brief The audible head extension (negative fade-in start), clamped so
     *        the extended read never goes before the source file's start.
     */
    double getHeadExtension(audium::TimeContextType context) const;

    /**
     * @brief The audible tail extension (negative fade-out end).
     */
    double getTailExtension(audium::TimeContextType context) const;

    /**
     * @brief The timeline span in which this clip produces audio: the region
     *        window widened by the fade extensions. Replaces
     *        getAbsolutePositionRange for scheduling decisions.
     */
    juce::Range<double> getAudibleRange(audium::TimeContextType context) const;

private:
    /**
     * @brief A shared pointer to the `TempoProvider` for tempo-related calculations.
     */
    std::shared_ptr<TempoProvider> tempoProvider;

public:
    /**
     * @brief The `DspClipData` object containing the clip's data.
     */
    DspClipData dspClipData;
};

} // namespace audium
