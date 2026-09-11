//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <algorithm>

namespace audium {

/**
 * The clip speed model shared by the playlist item and the DSP snapshot.
 *
 * A speed ratio s plays the source s times faster on 1/s of the timeline.
 * The range is load-bearing: StretchAudioSource sizes its scratch buffers
 * from maxSpeedRatio, so nothing may hand the audio thread a ratio outside
 * it.
 *
 * A tempo-locked clip does not store a ratio; it stores its own tempo and
 * derives the ratio from the project tempo, so it stays on the grid when
 * the project tempo moves.
 */
namespace ClipSpeed {

constexpr double minSpeedRatio = 0.25;
constexpr double maxSpeedRatio = 4.0;

/// Sanity bounds for a clip's native tempo (BPM); 0 means "unknown".
constexpr double minClipTempo = 20.0;
constexpr double maxClipTempo = 999.0;

inline double clampSpeedRatio (double ratio) noexcept
{
    return std::clamp (ratio, minSpeedRatio, maxSpeedRatio);
}

inline double clampClipTempo (double tempo) noexcept
{
    return std::clamp (tempo, minClipTempo, maxClipTempo);
}

/// The ratio a tempo-locked clip plays at: projectTempo / clipTempo, clamped
/// to the speed range. 1.0 while either tempo is unknown, so a locked clip
/// whose tempo has not been detected yet plays as recorded.
inline double tempoLockedRatio (double projectTempo, double clipTempo) noexcept
{
    if (projectTempo <= 0.0 || clipTempo <= 0.0)
        return 1.0;

    return clampSpeedRatio (projectTempo / clipTempo);
}

} // namespace ClipSpeed

} // namespace audium
