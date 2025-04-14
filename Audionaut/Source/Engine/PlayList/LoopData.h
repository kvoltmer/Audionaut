//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace audium {

/**
 * @struct LoopData
 * @brief Represents the configuration and state of a playback loop.
 *
 * The `LoopData` structure stores information about the loop's start and end positions,
 * its minimum length, and whether the loop is currently active. It is designed to be
 * serializable using the `nlohmann::json` library for easy persistence and restoration.
 */
struct LoopData
{
    double loopStartPositionClocks  = 96.0; ///< The start position of the loop in clock units.
    double loopEndPositionClocks    = 288.0; ///< The end position of the loop in clock units.
    double minimumLoopLengthClocks  = 1.0; ///< The minimum allowable length of the loop in clock units.
    bool loopActive                 = false; ///< Indicates whether the loop is currently active.
};

// Macro to define JSON serialization and deserialization for `LoopData`.
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(LoopData,
                                                loopStartPositionClocks,
                                                loopEndPositionClocks,
                                                loopActive);

} // namespace audium
