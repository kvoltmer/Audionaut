/*
  ==============================================================================

    ChannelMapping.cpp
    Created: 11 Nov 2024 11:45:49am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#include "ChannelMapping.h"

namespace audium
{

void ChannelMapping::clear()
{
    remappedChannels.clear();
}

void ChannelMapping::setOutputChannelMapping (const int sourceIndex, const int destIndex)
{
    while (remappedChannels.size() < sourceIndex)
        remappedChannels.add (-1);
    
    remappedChannels.set (sourceIndex, destIndex);
    
    if (destIndex >= 0)
    {
        auto src = sourceIndex;
        auto dst = getRemappedChannel(sourceIndex);
        
        std::cout << "source: " << src << " dest: " << dst << std::endl;
        
        auto src2  = getSourceChannel(destIndex);
        jassert(src == src2);
    }
}


int ChannelMapping::getRemappedChannel (const int sourceChannelIndex) const
{
    if (sourceChannelIndex >= 0 && sourceChannelIndex < remappedChannels.size())
        return remappedChannels.getUnchecked (sourceChannelIndex);
    
    return -1;
}

int ChannelMapping::getSourceChannel (const int destChannelIndex) const
{
    for (int sourceChannel = 0; sourceChannel < remappedChannels.size(); sourceChannel++)
    {
        if (remappedChannels.getUnchecked (sourceChannel) == destChannelIndex)
            return sourceChannel;
    }

    return -1;
}


bool ChannelMapping::anyOutputMapping() const
{
    if (remappedChannels.size() > 0)
    {
        for (auto i = 0; i < remappedChannels.size(); i++)
        {
            if (getRemappedChannel(i) >= 0)
                return true;
        }
    }
    
    return false;
}

void ChannelMapping::decrementChannelMapping(int startChannelNumber)
{
    for (auto i = 0; i < remappedChannels.size(); i++)
    {
        auto dest = getRemappedChannel(i);
        if (dest >= startChannelNumber)
        {
            auto newDest = dest - 1;
            jassert(newDest >= 0);
            std::cout << "remapping " << i << " -> " << newDest << std::endl;
            setOutputChannelMapping(i, newDest);
        }
    }
}

bool ChannelMapping::writeToJson (json& output)
{
    std::vector<int> mapping;
    jassert(remappedChannels.size() >= 0);
    for (auto i = 0; i < remappedChannels.size(); i++) {
        mapping.push_back(getRemappedChannel(i));
    }
    output["channel_mapping"] = mapping;
    return true;
}

bool ChannelMapping::readFromJson (json& input, bool rebuild)
{
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

