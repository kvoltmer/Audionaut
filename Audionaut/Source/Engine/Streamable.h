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

class Streamable
{

public:
    virtual ~Streamable() = default;
    
    // json used as default implementation
    virtual bool writeToStream (juce::OutputStream& outputStream)
    {
        json jout;
        auto result = writeToJson(jout);
        outputStream.writeString(jout.dump(2));
        return result;
    }
    
    // json used as default implementation
    virtual bool readFromStream (juce::InputStream& inputStream, bool rebuild = true)
    {
        auto inputString = inputStream.readString().toStdString();
        if (inputString.empty())
            throw std::runtime_error("empty input string");
    
        auto json = json::parse(inputString);
        return readFromJson(json, rebuild);
    }

    virtual bool writeToJson (json& /*output*/) { return false; }
    virtual bool readFromJson (json& /*input*/, bool /*rebuild*/) { return false; }

    
    virtual int getSizeInUnits() = 0;
    
    const int fileVersion = 1;
};

} // namespace audium
