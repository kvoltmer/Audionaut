//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace audium
{

/**
 * @struct AudioChannelData
 * @brief Represents the data for an audio channel, including visual and audio properties.
 *
 * This structure holds information about an audio channel's height, gain, pan, mute, and solo states.
 * It also provides serialization and deserialization methods for JSON conversion.
 */
class AudioChannelData
{
public:
    
    int height  = 100;   ///< The height of the audio channel in pixels.
    float gain  = 1.0f;  ///< The gain level of the audio channel (default is 1.0f).
    float pan   = 0.f;   ///< The pan position of the audio channel (-1.0f for left, 1.0f for right).
    bool mute   = false; ///< Indicates whether the audio channel is muted.
    bool solo   = false; ///< Indicates whether the audio channel is in solo mode.
    bool record = false; ///< Indicates whether the audio channel is in record mode.
    bool monitor = false; ///< Indicates whether the audio channel is in monitor mode.
    int channelNumber = -1; /// The  channel number of the audio track.
    int trackId = -1; /// The audio track id.
};

/**
 * @brief Serializes an AudioChannelData object to JSON.
 * @param j The JSON object to populate.
 * @param data The AudioChannelData object to serialize.
 */
inline void to_json(json& j, const AudioChannelData& data) {
    j = json{   {"height", data.height},
        {"gain", data.gain},
        {"pan", data.pan},
        {"mute", data.mute},
        {"solo", data.solo},
        {"record", data.record},
        {"monitor", data.monitor}
    };
}

/**
 * @brief Deserializes a JSON object to an AudioChannelData object.
 * @param j The JSON object to deserialize.
 * @param data The AudioChannelData object to populate.
 */
inline void from_json(const json& j, AudioChannelData& data) {
    
    if (j.contains("height"))
        data.height = j.at("height").get<int>();
    
    if (j.contains("gain"))
    
        data.gain = j.at("gain").get<float>();
    if (j.contains("pan"))
        data.pan = j.at("pan").get<float>();
    
    if (j.contains("mute"))
        data.mute = j.at("mute").get<bool>();
    
    if (j.contains("solo"))
        data.solo = j.at("solo").get<bool>();
    
    if (j.contains("record"))
        data.record = j.at("record").get<bool>();
    
    if (j.contains("monitor"))
        data.monitor = j.at("monitor").get<bool>();
    
}

} // namespace audium
