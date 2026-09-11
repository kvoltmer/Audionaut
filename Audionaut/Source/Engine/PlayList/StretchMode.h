//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

namespace audium {

/**
 * How a clip's speed ratio is realised.
 *
 * RePitch is plain varispeed: the resampler in the clip's playback chain
 * runs the source faster or slower, changing pitch and length together.
 * Stretch keeps the pitch by running the StretchAudioSource node behind
 * the resampler, which then only corrects the file's sample rate - see
 * ClipTransportSource::updateSpeedChain.
 */
enum class StretchMode
{
    RePitch = 0,
    Stretch = 1
};

} // namespace audium
