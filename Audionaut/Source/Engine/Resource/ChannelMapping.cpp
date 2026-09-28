//    Audionaut - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Audionaut uses a GPL/commercial licence - see LICENCE.md for details.

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
        // std::cout << this << " mapping src:" << sourceChannel << " dst: " << destinationChannel << std::endl;
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

bool ChannelMapping::readFromJson (json& input, bool /*rebuild*/)
{
    // legacy mapping: an array of destination channels indexed by source channel
    if (input.contains("channel_mapping")) {
        const auto& mapping = input.at("channel_mapping");
        if (! mapping.is_array())
            return false;

        for (const auto& destIndex : mapping)
            if (! destIndex.is_number_integer())
                return false;

        clear();
        auto counter = 0;
        for (const auto& destIndex : mapping)
            setOutputChannelMapping(counter++, destIndex.get<int>());

        return true;
    }

    // current format, as written by writeToJson
    if (! input.contains("source_channel") || ! input.contains("destination_channel"))
        return false;

    const auto& source      = input.at("source_channel");
    const auto& destination = input.at("destination_channel");
    if (! source.is_number_integer() || ! destination.is_number_integer())
        return false;

    setSourceChannel(source.get<int>());
    setDestinationChannel(destination.get<int>());
    return true;
}

} // namespace audium

