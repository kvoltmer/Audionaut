/*
  ==============================================================================

    AudioChannel.h
    Created: 22 Dec 2023 11:34:17am
    Author:  Klaus Voltmer

  ==============================================================================
*/

#pragma once

#include "Engine/Resource/AudioResource.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Channel/AudioChannelData.h"
#include "Engine/Selection/Selectable.h"
#include "Engine/Selection/SelectionManager.h"

class AudioChannel : public audium::Selectable
{
    
public:
    AudioChannel(AudioTrack &audioTrack,
                 std::shared_ptr<audium::SelectionManager> selectionManager) :
        audium::Selectable(selectionManager),
        audioTrack(audioTrack)
    {
    }
    
    int getChannelHeight() const noexcept {
        return data.height;
    }
    void setChannelHeight(const int height) {
        data.height = height;
    }
    
    void setGain(const float new_gain) {
        data.gain = new_gain;
    }
    float getGain() const noexcept {
        return data.gain;
    }

    int getChannelNumber() const {
        auto channel = std::dynamic_pointer_cast<const AudioChannel>(getSharedPtr());
        return audioTrack.audioChannelContainer->getIndex(channel);
    }
    
    AudioChannelData data;
    
    AudioTrack &getAudioTrack() const { return audioTrack; }
    
private:
    AudioTrack &audioTrack;
    
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioChannel)
    
};
