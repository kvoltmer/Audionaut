//    Lola - Audio editing application for multitrack recordings.
//    Copyright (C) 2025 Klaus Voltmer
//
//    This program is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "Engine/Resource/AudioResource.h"
#include "Engine/Group/AudioTrack.h"
#include "Engine/Channel/AudioChannelData.h"
#include "Engine/Selection/Selectable.h"
#include "Engine/Selection/SelectionManager.h"
#include "Engine/Playback/AudioBusInterface.h"

class AudioChannel : public audium::Selectable
{
    
public:
    AudioChannel(AudioTrack &audioTrack_,
                 std::shared_ptr<audium::SelectionManager> selectionManager_,
                 std::shared_ptr<audium::AudioBusInterface> audioBusInterface_) :
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
    
    std::shared_ptr<audium::AudioBusInterface> audioBusInterface;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioChannel)
    
};
