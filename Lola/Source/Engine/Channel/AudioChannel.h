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
#include "Engine/Playback/AudioBusRenderer.h"

class AudioChannel : public audium::Selectable
{
    
public:
    AudioChannel(AudioTrack &audioTrack_,
                 std::shared_ptr<audium::SelectionManager> selectionManager_,
                 std::shared_ptr<audium::LockFreeCommander> lockFreeCommander_,
                 std::shared_ptr<audium::AudioBusRenderer<float>> audioBusRenderer_) :
        audioTrack(audioTrack_),
        audium::Selectable(selectionManager_),
        lockFreeCommander(lockFreeCommander_),
        audioBusRenderer(audioBusRenderer_)
    {
    }
    
    int getChannelHeight() const noexcept {
        return data.height;
    }
    void setChannelHeight(const int height) {
        data.height = height;
    }
    
    void setGain(const float new_gain);
    float getGain() const noexcept;
    
    void setPan(const float new_pan);
    float getPan() const noexcept;
    
    void commitChannelData();

    int getChannelNumber() const {
        auto channel = std::dynamic_pointer_cast<const AudioChannel>(getSharedPtr());
        return audioTrack.audioChannelContainer->getIndex(channel);
    }
    
    AudioChannelData data;
    
    AudioTrack &getAudioTrack() const { return audioTrack; }
    
private:
    AudioTrack &audioTrack;
    
    std::shared_ptr<audium::LockFreeCommander> lockFreeCommander;
    std::shared_ptr<audium::AudioBusRenderer<float>> audioBusRenderer;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioChannel)
    
};
