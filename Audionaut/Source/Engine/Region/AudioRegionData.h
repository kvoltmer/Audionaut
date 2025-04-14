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
 * @struct AudioRegionData
 * @brief Represents metadata for an audio region.
 *
 * The `AudioRegionData` struct contains information about an audio region, including
 * its name, time range, identifiers, and gain values for each channel.
 */
struct AudioRegionData
{
    /**
     * @typedef tRange
     * @brief A type alias for a range of doubles, representing the start and end times of the region.
     */
    typedef class juce::Range<double> tRange;

    /**
     * @brief The name of the audio region.
     */
    std::string name;

    /**
     * @brief The time range of the audio region in seconds.
     */
    tRange range;

    /**
     * @brief The unique identifier for the region.
     */
    int region_id = -1;

    /**
     * @brief The unique identifier for the associated track.
     */
    int track_id = -1;

    /**
     * @brief The unique identifier for the associated resource group.
     */
    int resource_group_id = -1;

    /**
     * @brief The gain values for each channel.
     */
    std::vector<double> gain_vector;
};

/**
 * @brief Serializes an `AudioRegionData` object to a JSON object.
 * @param j The JSON object to write to.
 * @param r The `AudioRegionData` object to serialize.
 */
inline void to_json(json& j, const AudioRegionData& r) {
    j = json{
        {"name", r.name},
        {"start", r.range.getStart()},
        {"end", r.range.getEnd()},
        {"id", r.region_id},
        {"track_id", r.track_id},
        {"resource_group_id", r.resource_group_id},
        {"gain_vector", r.gain_vector},
    };
}

/**
 * @brief Deserializes an `AudioRegionData` object from a JSON object.
 * @param j The JSON object to read from.
 * @param r The `AudioRegionData` object to populate.
 */
inline void from_json(const json& j, AudioRegionData& r) {
    j.at("name").get_to(r.name);
    r.range.setStart(j.at("start").get<double>());
    r.range.setEnd(j.at("end").get<double>());

    if (j.contains("id"))
        j.at("id").get_to(r.region_id);

    if (j.contains("track_id"))
        j.at("track_id").get_to(r.track_id);

    if (j.contains("sub_group_id")) // legacy
        j.at("sub_group_id").get_to(r.resource_group_id);

    if (j.contains("resource_group_id"))
        j.at("resource_group_id").get_to(r.resource_group_id);

    if (j.contains("gain_vector"))
        j.at("gain_vector").get_to(r.gain_vector);
}

} // namespace audium
