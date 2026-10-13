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
 * @class Streamable
 * @brief An interface for objects that can be serialized to and deserialized from streams or JSON.
 *
 * The `Streamable` class provides a mechanism for objects to be written to and read from
 * JUCE streams or JSON objects. It includes default implementations for stream-based
 * serialization using JSON as the intermediate format.
 */
class Streamable
{
public:
    /**
     * @brief Virtual destructor.
     */
    virtual ~Streamable() = default;

    /**
     * @brief Writes the object to a JUCE output stream.
     *
     * This method uses JSON as the default implementation for serialization.
     *
     * @param outputStream The output stream to write to.
     * @return True if the operation was successful, false otherwise.
     */
    virtual bool writeToStream(juce::OutputStream& outputStream)
    {
        json jout;
        auto result = writeToJson(jout);
        outputStream.writeString(jout.dump(2));
        return result;
    }


    /**
     * @brief Reads the object from a JUCE input stream.
     *
     * This method uses JSON as the default implementation for deserialization.
     *
     * @param inputStream The input stream to read from.
     * @param rebuild Whether to rebuild the object during deserialization.
     * @return True if the operation was successful, false otherwise.
     * @throws std::runtime_error If the input string is empty or invalid.
     */
    virtual bool readFromStream(juce::InputStream& inputStream, bool rebuild = true)
    {
        auto inputString = inputStream.readString().toStdString();
        if (inputString.empty())
            throw std::runtime_error("empty input string");
    
        auto json = json::parse(inputString);
        return readFromJson(json, rebuild);
    }

    /**
     * @brief Writes the object to a JSON object.
     *
     * This method should be overridden by derived classes to provide custom
     * JSON serialization logic.
     *
     * @param output The JSON object to write to.
     * @return True if the operation was successful, false otherwise.
     */
    virtual bool writeToJson (json& /*output*/) { return false; }

    /**
     * @brief Reads the object from a JSON object.
     *
     * This method should be overridden by derived classes to provide custom
     * JSON deserialization logic.
     *
     * @param input The JSON object to read from.
     * @param rebuild Whether to rebuild the object during deserialization.
     * @return True if the operation was successful, false otherwise.
     */
    virtual bool readFromJson (json& /*input*/, bool /*rebuild*/) { return false; }
};

} // namespace audium
