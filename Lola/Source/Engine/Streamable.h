/*
  ==============================================================================

    Streamable.h
    Created: 1 Feb 2024 1:40:08pm
    Author:  Klaus Voltmer

  ==============================================================================
*/

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

        json data = json::parse(inputString);
        
//        json compare;
//        writeToJson(compare);
//        if (compare == data)
//        {
//            std::cout << "data is equal" << std::endl;
//        }
        
        return readFromJson(data, rebuild);
    }

    virtual bool writeToJson (json& output) { return false; }
    virtual bool readFromJson (json& input, bool rebuild) { return false; }

    
    virtual int getSizeInUnits() = 0;
    
    const int fileVersion = 1;
};

} // namespace audium
