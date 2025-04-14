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
 * @struct AudioClipData
 * @brief Represents the data associated with an audio clip.
 *
 * The `AudioClipData` structure contains information about the absolute position
 * of the audio clip in transport clocks and its region data (start and end times).
 * It also provides serialization and deserialization methods for JSON.
 */
struct AudioClipData
{
    typedef class juce::Range<double> tRange;

    double absolutePositionClocks = 0.0; ///< The absolute transport position in clocks.
    tRange regionData; ///< The start and end times of the audio clip in seconds.
};

/**
 * @brief Serializes an `AudioClipData` object to JSON.
 * @param j The JSON object to write to.
 * @param d The `AudioClipData` object to serialize.
 */
inline void to_json(json& j, const AudioClipData& d) {
    j = json{
        {"position_clocks", d.absolutePositionClocks},
        {"start", d.regionData.getStart()},
        {"end", d.regionData.getEnd()}
    };
}

/**
 * @brief Deserializes an `AudioClipData` object from JSON.
 * @param j The JSON object to read from.
 * @param d The `AudioClipData` object to populate.
 */
inline void from_json(const json& j, AudioClipData& d) {
    j.at("position_clocks").get_to(d.absolutePositionClocks);
    d.regionData.setStart(j.at("start").get<double>());
    d.regionData.setEnd(j.at("end").get<double>());
}

} // namespace audium
