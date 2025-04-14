//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include <JuceHeader.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace audium
{

/**
 * @class ChannelMapping
 * @brief Manages the mapping between source and destination audio channels.
 *
 * The `ChannelMapping` class provides functionality to map audio channels
 * from a source to a destination, clear mappings, and serialize/deserialize
 * mappings to/from JSON.
 */
class ChannelMapping
{
public:
    /**
     * @brief Constructs a default `ChannelMapping` object.
     */
    ChannelMapping() = default;

    /**
     * @brief Destroys the `ChannelMapping` object.
     */
    ~ChannelMapping() = default;

    /**
     * @brief Clears all channel mappings.
     */
    void clear();

    /**
     * @brief Sets the mapping between a source channel and a destination channel.
     * @param sourceChannel The source channel index.
     * @param destChannel The destination channel index.
     */
    void setOutputChannelMapping(int sourceChannel, int destChannel);

    /**
     * @brief Retrieves the destination channel index.
     * @return The destination channel index.
     */
    int getDestinationChannel() const;

    /**
     * @brief Sets the destination channel index.
     * @param newDestChannel The new destination channel index.
     */
    void setDestinationChannel(int newDestChannel);

    /**
     * @brief Retrieves the source channel index.
     * @return The source channel index.
     */
    int getSourceChannel() const;

    /**
     * @brief Sets the source channel index.
     * @param newSrcChannel The new source channel index.
     */
    void setSourceChannel(int newSrcChannel);

    /**
     * @brief Checks if the mapping contains the specified source channel number.
     * @param channelNumber The source channel number to check.
     * @return True if the source channel number exists, false otherwise.
     */
    bool containsSourceChannelNumber(int channelNumber) const;

    /**
     * @brief Checks if the mapping contains the specified destination channel number.
     * @param channelNumber The destination channel number to check.
     * @return True if the destination channel number exists, false otherwise.
     */
    bool containsDestinationChannelNumber(int channelNumber) const;

    /**
     * @brief Deletes the mapping for the specified source channel index.
     * @param sourceChannelIndex The source channel index to delete.
     * @return True if the mapping was successfully deleted, false otherwise.
     */
    bool deleteChannel(int sourceChannelIndex);

    /**
     * @brief Decrements the destination channel indices starting from a specified channel number.
     * @param startChannelNumber The starting channel number for decrementing.
     */
    void decrementDestinationChannel(int startChannelNumber);

    /**
     * @brief Serializes the channel mapping to a JSON object.
     * @param output The JSON object to write to.
     * @return True if the serialization was successful, false otherwise.
     */
    bool writeToJson(json& output);

    /**
     * @brief Deserializes the channel mapping from a JSON object.
     * @param input The JSON object to read from.
     * @param rebuild Whether to rebuild the mapping after reading.
     * @return True if the deserialization was successful, false otherwise.
     */
    bool readFromJson(json& input, bool rebuild);

private:
    /**
     * @brief The source channel index.
     */
    int srcChannel = -1;

    /**
     * @brief The destination channel index.
     */
    int dstChannel = -1;

    /**
     * @brief JUCE macro to prevent copying and detect memory leaks.
     */
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChannelMapping)
};


} // namespace audium
