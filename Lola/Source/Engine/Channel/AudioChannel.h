//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    Lola uses a GPL/commercial licence - see LICENCE.md for details.

#pragma once

#include "Engine/Resource/AudioResource.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Channel/AudioChannelData.h"
#include "Engine/Selection/Selectable.h"
#include "Engine/Selection/SelectionManager.h"
#include "Engine/Playback/AudioBusInterface.h"

namespace audium
{

class AudioChannel : public audium::Selectable
{
    
public:
    AudioChannel(AudioTrack &audioTrack_,
                 std::shared_ptr<SelectionManager> selectionManager_,
                 std::shared_ptr<AudioBusInterface> audioBusInterface_) :
    audium::Selectable(selectionManager_),
    audioTrack(audioTrack_),
    audioBusInterface(audioBusInterface_)
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
    
    void setMute(bool bMute);
    bool getMute() const noexcept;
    
    void setSolo(bool bSolo);
    bool getSolo() const noexcept;
    
    void commitChannelData();
    
    int getChannelNumber() const {
        auto channel = std::dynamic_pointer_cast<const AudioChannel>(getSharedPtr());
        return audioTrack.audioChannelContainer->getIndex(channel);
    }
    
    AudioChannelData data;
    
    AudioTrack &getAudioTrack() const { return audioTrack; }
    
private:
    AudioTrack &audioTrack;
    
    std::shared_ptr<AudioBusInterface> audioBusInterface;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioChannel)
    
};

} // namespace audium

