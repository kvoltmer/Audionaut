/*
  ==============================================================================

    AudioChannel.h
    Created: 22 Dec 2023 11:34:17am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include "Engine/AudioResource.h"
#include "Engine/Group/AudioGroup.h"

class AudioChannel
{
    
public:
    AudioChannel(AudioGroup &audioGroup) :
        audioGroup(audioGroup)
    {
    }
    
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
    
    void setSelected(bool bSelected) { selected = bSelected; }
    bool isSelected() const { return selected; }

    int getChannelNumber()
    {
        return audioGroup.getChannelNumberFor(this);
    }
    
private:
    AudioGroup &audioGroup;
    //std::shared_ptr<AudioResource> audioResource;
    int channelHeight = 100;
    bool selected = false;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioChannel)
    
};
