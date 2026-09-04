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
 * Stretch (not implemented yet) will keep the pitch by routing the clip
 * through a time-stretch node at the same seam - see the resampler assembly
 * in ClipTransportSource::setSource.
 */
enum class StretchMode
{
    RePitch = 0,
    Stretch = 1   // reserved; no backend yet
};

} // namespace audium
