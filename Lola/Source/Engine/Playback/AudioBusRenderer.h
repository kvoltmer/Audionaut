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
#include "PlaybackDefines.h"

class AudioTrackContainer;

namespace audium
{

class Playback;

template <class SampleType>
class AudioBusRenderer {
    
    
public:
    AudioBusRenderer(std::shared_ptr<audium::Playback> playback_) :
        playback(playback_)
    {
        setMasterGain(1.f);
        reset();
    }
    
    ~AudioBusRenderer() = default;
    
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate);
    
    void setNumAudioBusChannels(int numChannels);
    
    void processAudioBlock(const juce::AudioSourceChannelInfo& outputInfo);
    
    void setPan(const int channelNumber, const SampleType newPan)
    {
        if (channelNumber >= 0 && channelNumber < MAX_AUDIO_CHANNELS) {
            // std::cout << "setPan " << channelNumber << " " << newPan << std::endl;
            panners[channelNumber].setPan(newPan);
        }
    }

    void setGain(const int channelNumber, const SampleType newGain)
    {
        if (channelNumber >= 0 && channelNumber < MAX_AUDIO_CHANNELS) {
            // std::cout << "setGain " << channelNumber << " " << newGain << std::endl;
            gains[channelNumber].setGainLinear(newGain);
            gainStates[channelNumber] = newGain;
        }
    }
    
    void setMute(const int channelNumber, const bool bMute)
    {
        if (channelNumber >= 0 && channelNumber < MAX_AUDIO_CHANNELS) {
            //std::cout << "setMute " << channelNumber << " " << bMute << std::endl;
            muteStates[channelNumber] = bMute;
            if (!soloStates[channelNumber])
                gains[channelNumber].setGainLinear(bMute ? 0.f : gainStates[channelNumber]);
        }
    }

    void setSolo(const int channelNumber, const bool bSolo)
    {
        if (channelNumber >= 0 && channelNumber < MAX_AUDIO_CHANNELS) {
            //std::cout << "setSolo " << channelNumber << " " << bSolo << std::endl;
            soloStates[channelNumber] = bSolo;

            auto anySolo = false;
            for (auto c = 0; c < MAX_AUDIO_CHANNELS; ++c) {
                if (soloStates[c]) {
                    anySolo = true;
                    break;
                }
            }
            
            for (auto c = 0; c < MAX_AUDIO_CHANNELS; ++c) {
                if (anySolo) {
                    if (soloStates[c])
                        gains[c].setGainLinear(gainStates[c]);
                    else
                        gains[c].setGainLinear(0.f);
                }
                else {
                    // mute stats apply
                    gains[c].setGainLinear(muteStates[c] ? 0.f : gainStates[c]);
                }
            }
        }
    }
    
    void setMasterGain(const float newGain)
    {
        masterGain.setGainLinear(newGain);
    }
    
    const float getChannelLevel(const int channelNumber) const
    {
        if (channelNumber >= 0 && channelNumber < MAX_AUDIO_CHANNELS)
            return channelLevel[channelNumber].load();
        
        return 0.f;
    }
    
    const float getMasterLevel(const int channelNumber) const
    {
        if (channelNumber >= 0 && channelNumber < 2)
            return masterLevel[channelNumber].load();
        
        return 0.f;
    }
    
    void reset()
    {
        for (auto c = 0; c < MAX_AUDIO_CHANNELS; ++c) {
            gainStates[c] = 1.0;
            muteStates[c] = false;
            soloStates[c] = false;
            channelLevel[c] = 0.0;
        }
    }
    
private:
    
    std::shared_ptr<audium::Playback> playback;
    
    juce::AudioBuffer<SampleType> audioBus;
    juce::AudioBuffer<SampleType> channelBuffer;
    juce::AudioBuffer<SampleType> stereoBuffer;
        
    juce::dsp::Panner<SampleType> panners[MAX_AUDIO_CHANNELS];
    juce::dsp::Gain<SampleType> gains[MAX_AUDIO_CHANNELS];
    juce::dsp::Gain<SampleType> masterGain;
    
    std::atomic<float> channelLevel[MAX_AUDIO_CHANNELS];
    std::atomic<float> masterLevel[2];
    
    SampleType gainStates[MAX_AUDIO_CHANNELS];
    bool muteStates[MAX_AUDIO_CHANNELS];
    bool soloStates[MAX_AUDIO_CHANNELS];
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioBusRenderer)

};

} // namespace audium
