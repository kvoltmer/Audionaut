//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.

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

    virtual bool writeToJson (json& output) { return false; }
    virtual bool readFromJson (json& input, bool rebuild) { return false; }

    
    virtual int getSizeInUnits() = 0;
    
    const int fileVersion = 1;
};

} // namespace audium
