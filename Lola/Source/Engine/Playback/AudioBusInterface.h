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

#include <JuceHeader.h>

#include "Engine/Playback/AudioBusRenderer.h"
#include "Engine/Core/LockFreeCommander.h"

namespace audium
{

// thread save audio bus interface
class AudioBusInterface
{
    
public:
    
    AudioBusInterface(std::shared_ptr<audium::LockFreeCommander> lockFreeCommander_,
                      std::shared_ptr<audium::AudioBusRenderer<float>> audioBusRenderer_) :
        lockFreeCommander(lockFreeCommander_),
        audioBusRenderer(audioBusRenderer_)
    {
    }
    
    ~AudioBusInterface() = default;
    
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate);
    
    void processAudio(const juce::AudioSourceChannelInfo& outputInfo);
    
    void setNumAudioBusChannels(int numChannels);
    
    void setPan(const int channelNumber, const float newPan);
    void setGain(const int channelNumber, const float newGain);
    void setMute(const int channelNumber, const bool bMute);
    void setSolo(const int channelNumber, const bool bSolo);
    
    void setMasterGain(const float newGain);
    
    const float getChannelLevel(const int channelNumber) const;
    const float getMasterLevel(const int channelNumber) const;

private:


    std::shared_ptr<audium::LockFreeCommander> lockFreeCommander;
    std::shared_ptr<audium::AudioBusRenderer<float>> audioBusRenderer;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioBusInterface)

};

} // namespace audium
