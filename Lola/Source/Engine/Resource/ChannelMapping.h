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
