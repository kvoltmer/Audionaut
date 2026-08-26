//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <atomic>

namespace audium {

/**
 * @class HeadlessMode
 * @brief Process-wide runtime switch for "no GUI, no audio device" behavior.
 *
 * The AUDIONAUT_HEADLESS compile-time define covers builds that are headless
 * by construction (tests, audionaut-cli). The GUI app cannot use that define
 * - it needs its message boxes and device handling - but its in-app CLI mode
 * runs the same engine code windowless. Engine sites whose behavior must
 * differ *at runtime* (message boxes, the lock-free command pump) consult
 * this flag instead of the define.
 *
 * Defaults to headless in AUDIONAUT_HEADLESS builds so the console targets
 * never need to set it; the GUI app flips it on only for the duration of a
 * CLI invocation.
 */
class HeadlessMode {
public:
    static void set (bool headless) { flag().store (headless); }
    static bool isHeadless()        { return flag().load(); }

private:
    static std::atomic<bool>& flag()
    {
       #if defined(AUDIONAUT_HEADLESS)
        static std::atomic<bool> value { true };
       #else
        static std::atomic<bool> value { false };
       #endif
        return value;
    }
};

} // namespace audium
