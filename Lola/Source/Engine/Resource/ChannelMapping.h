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

class ChannelMapping
{
public:
    
    ChannelMapping() = default;
    
    ~ChannelMapping() = default;
    
    void clear();
    
    void setOutputChannelMapping (int sourceChannel,
                                  int destChannel);

    int getDestinationChannel() const;
    void setDestinationChannel(int newDestChannel);
    
    int getSourceChannel() const;
    void setSourceChannel(int newSrcChannel);
    
    bool containsSourceChannelNumber(int channelNumber) const;
    bool containsDestinationChannelNumber(int channelNumber) const;

    bool deleteChannel(int sourceChannelIndex);
    void decrementDestinationChannel(int startChannelNumber);

    bool writeToJson (json& output);
    bool readFromJson (json& input, bool rebuild);

private:
    
    int srcChannel = -1;
    int dstChannel = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelMapping)
};


} // namespace audium
