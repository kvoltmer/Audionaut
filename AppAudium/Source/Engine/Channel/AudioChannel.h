/*
  ==============================================================================

    AudioChannel.h
    Created: 22 Dec 2023 11:34:17am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include "Engine/AudioResource.h"

class AudioChannel {
    
    //AudioChannel(std::shared_ptr<AudioResource> audioResource)
    
public:
    int getChannelHeight() const { return channelHeight; }
    void setChannelHeight(int height) { channelHeight = height; }
    
    bool writeToStream (juce::OutputStream& outputStream)
    {
        outputStream.writeInt(channelHeight);
        return true;
    }
    
    bool readFromStream (juce::InputStream& inputStream)
    {
        auto height = inputStream.readInt();
        channelHeight = height;
        return true;
    }
    
private:
    //std::shared_ptr<AudioResource> audioResource;
    int channelHeight = 100;
    
};
