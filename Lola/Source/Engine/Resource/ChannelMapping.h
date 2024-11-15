/*
  ==============================================================================

    ChannelMapping.h
    Created: 11 Nov 2024 11:46:03am
    Author:  Klaus Voltmer

  ==============================================================================
*/

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
    
    void setOutputChannelMapping (int sourceChannelIndex,
                                  int destChannelIndex);

    int getRemappedChannel (int sourceChannelIndex) const;
    int getSourceChannel (int destChannelIndex) const;


    bool anyOutputMapping() const;

    void decrementChannelMapping(int startChannelNumber);

    bool writeToJson (json& output);
    bool readFromJson (json& input, bool rebuild);

    const juce::Array<int> getChannelMapping() const { return remappedChannels; }

private:

    juce::Array<int> remappedChannels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelMapping)
};


} // namespace audium
