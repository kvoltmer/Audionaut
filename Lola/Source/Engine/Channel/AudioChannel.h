/*
  ==============================================================================

    AudioChannel.h
    Created: 22 Dec 2023 11:34:17am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include "Engine/Resource/AudioResource.h"
#include "Engine/Group/AudioGroup.h"
#include "Engine/Channel/AudioChannelData.h"

class AudioChannel
{
    
public:
    AudioChannel(AudioGroup &audioGroup, int channelNumber) :
        audioGroup(audioGroup),
        channelNumber(channelNumber)
    {
    }
    
    int getChannelHeight() const { return data.height; }
    void setChannelHeight(int height) { data.height = height; }
        
    void setSelected(bool bSelected) { data.selected = bSelected; }
    bool isSelected() const { return data.selected; }

    int getChannelNumber() const
    {
        return channelNumber;
    }
    
    void setChannelNumber(int number)
    {
        channelNumber = number;
    }
    
    AudioChannelData data;
    
private:
    AudioGroup &audioGroup;
    
    int channelNumber = 0;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioChannel)
    
};
