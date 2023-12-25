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
    
private:
    //std::shared_ptr<AudioResource> audioResource;
    int channelHeight = 100;
    
};
