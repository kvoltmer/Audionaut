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
 * @struct AudioResourceData
 * @brief Represents metadata for an audio resource.
 *
 * The `AudioResourceData` struct stores information about an audio resource, including
 * its URL, gain, and channel position. It is used for serialization and deserialization
 * of audio resource data.
 */
struct AudioResourceData
{
    /**
     * @typedef tRange
     * @brief A type alias for a JUCE range of doubles.
     */
    typedef class juce::Range<double> tRange;

    /**
     * @brief The URL of the audio resource.
     */
    std::string url;

    /**
     * @brief The gain value of the audio resource.
     */
    float gain;

    /**
     * @brief The channel position of the audio resource.
     */
    int channelPos;
};

/**
 * @brief Defines the JSON serialization and deserialization for `AudioResourceData`.
 */
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(AudioResourceData,
                                   url,
                                   gain,
                                   channelPos);

} // namespace audium
