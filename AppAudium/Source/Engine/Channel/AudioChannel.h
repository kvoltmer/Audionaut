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
    AudioChannel(AudioGroup &audioGroup) :
        audioGroup(audioGroup)
    {
    }
    
    int getChannelHeight() const { return data.height; }
    void setChannelHeight(int height) { data.height = height; }
        
    void setSelected(bool bSelected) { data.selected = bSelected; }
    bool isSelected() const { return data.selected; }

    int getChannelNumber()
    {
        return audioGroup.getChannelNumberFor(this);
    }
    
    AudioChannelData data;
    
private:
    AudioGroup &audioGroup;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioChannel)
    
};
