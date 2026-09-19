//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <atomic>

namespace audium {

/**
 * Whether the voices are rendering offline (a bounce) or live.
 *
 * The clip chain reads through a BufferingAudioSource whose background
 * thread fills the read-ahead after every position jump. Live, a voice may
 * wait a couple of milliseconds for the next block and otherwise plays
 * silence rather than stall the callback; offline there is no callback
 * deadline, so waiting for the reader is the right thing and keeps the
 * bounce sample-accurate. AudioExporter flips this around a bounce.
 */
namespace RenderTiming
{
    inline std::atomic<bool> offlineRendering { false };

    inline void setOffline (bool shouldRenderOffline) noexcept   { offlineRendering.store (shouldRenderOffline); }
    inline bool isOffline() noexcept                             { return offlineRendering.load(); }

    /// How long a voice may block waiting for its read-ahead to catch up.
    inline unsigned int inputReadinessTimeoutMs() noexcept       { return isOffline() ? 5000u : 2u; }
}

} // namespace audium
