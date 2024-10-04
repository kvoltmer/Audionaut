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
#include "Engine/Selection/Selectable.h"
#include "Engine/Selection/SelectionManager.h"

class AudioChannel : public audium::Selectable
{
    
public:
    AudioChannel(AudioGroup &audioGroup,
                 int channelNumber,
                 std::shared_ptr<audium::SelectionManager> selectionManager) :
        audium::Selectable(selectionManager),
        audioGroup(audioGroup),
        channelNumber(channelNumber)
    {
    }
    
    int getChannelHeight() const { return data.height; }
    void setChannelHeight(int height) { data.height = height; }

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
