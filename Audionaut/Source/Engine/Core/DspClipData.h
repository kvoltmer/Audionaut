//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include "Engine/Group/AudioClipData.h"

namespace audium {

/**
 * @struct DspClipData
 * @brief Represents the data associated with a DSP clip.
 *
 * The `DspClipData` struct contains information about an audio clip, including
 * its gain, fade-in/out durations, and transport source index. It is used to
 * manage DSP-related properties of audio clips in the Audionaut application.
 */
struct DspClipData
{
    /**
     * @brief The audio clip data.
     *
     * Contains the core audio data and metadata for the clip.
     */
    AudioClipData clipData;
    
    /**
     * @brief Indicates whether the clip is active.
     *
     * A boolean flag that determines if the clip is currently active.
     */
    bool active = false;
    
    /**
     * @brief The gain applied to the clip.
     *
     * A floating-point value representing the gain multiplier for the clip.
     * Default is 1.0 (no gain adjustment).
     */
    float clipGain = 1.f;
    
    /**
     * @brief The fade-in duration of the clip in clock units.
     *
     * A double value representing the duration of the fade-in effect.
     */
    double clipFadeInClocks = 0.0;
    
    /**
     * @brief The fade-out duration of the clip in clock units.
     *
     * A double value representing the duration of the fade-out effect.
     */
    double clipFadeOutClocks = 0.0;

    /**
     * @brief The fade-in ramp start offset from the clip start, in clocks.
     *
     * Signed: positive = the ramp begins inside the clip (silent head before
     * it), negative = the ramp begins before the clip - the fade extends the
     * audible material with source audio from before the region window.
     */
    double clipFadeInStartClocks = 0.0;

    /**
     * @brief The fade-out ramp end offset from the clip end, in clocks.
     *
     * Signed: positive = the ramp ends inside the clip (silent tail after
     * it), negative = the ramp ends past the clip - the fade extends the
     * audible material past the region window.
     */
    double clipFadeOutEndClocks = 0.0;
    
    /**
     * @brief The curve exponents of the two fade ramps.
     *
     * 0.5 = sqrt = equal power (default), 1 = linear, > 1 = exponential.
     */
    double clipFadeInCurve = 0.5;
    double clipFadeOutCurve = 0.5;

    /**
     * @brief The transport source index for the clip.
     *
     * An integer representing the index of the transport source associated
     * with the clip. Default is -1 (no source assigned).
     */
    int voiceSourceIndex = -1;
};

} // namespace audium
