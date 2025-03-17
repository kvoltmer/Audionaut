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

#include "ChannelMapping.h"

namespace audium
{

void ChannelMapping::clear()
{
    srcChannel = -1;
    dstChannel = -1;
}

void ChannelMapping::setOutputChannelMapping (const int sourceChannel,
                                              const int destinationChannel)
{
    if (destinationChannel >= 0) {
        std::cout << this << " mapping src:" << sourceChannel << " dst: " << destinationChannel << std::endl;
        srcChannel = sourceChannel;
        dstChannel = destinationChannel;
    }
}

int ChannelMapping::getDestinationChannel() const
{
    return dstChannel;
}

void ChannelMapping::setDestinationChannel(int newDestChannel)
{
    dstChannel = newDestChannel;
}

int ChannelMapping::getSourceChannel() const
{
    return srcChannel;
}

void ChannelMapping::setSourceChannel(int newSrcChannel)
{
    srcChannel = newSrcChannel;
}

bool ChannelMapping::containsSourceChannelNumber(int channelNumber) const
{
    return (channelNumber == srcChannel);
}

bool ChannelMapping::containsDestinationChannelNumber(int channelNumber) const
{
    return (channelNumber == dstChannel);
}

bool ChannelMapping::deleteChannel(const int destIndex)
{
    if (containsDestinationChannelNumber(destIndex)) {
        setDestinationChannel(-1);
        return true;
    }
    return false;
}

void ChannelMapping::decrementDestinationChannel(int startChannelNumber)
{
    auto dest = getDestinationChannel();
    if (dest >= startChannelNumber) {
        auto newDest = dest - 1;
        jassert(newDest >= 0);
        std::cout << "remapping " << srcChannel << " -> " << newDest << std::endl;
        setDestinationChannel(newDest);
    }
}

bool ChannelMapping::writeToJson (json& output)
{
    output["destination_channel"] = dstChannel;
    output["source_channel"] = srcChannel;
    return true;
}

bool ChannelMapping::readFromJson (json& input, bool rebuild)
{
    if (input.contains("source_channel"))
        setSourceChannel(input.at("source_channel").get<int>());

    if (input.contains("destination_channel"))
        setDestinationChannel(input.at("destination_channel").get<int>());
    
    // legacy mapping
    if (input.contains("channel_mapping")) {
        clear();
        auto counter = 0;
        std::vector<int> mapping = input["channel_mapping"];
        for (auto destIndex : mapping)
            setOutputChannelMapping(counter++, destIndex);

        return true;
    }
    return false;
}

} // namespace audium

